#include "OrderSystem.h"
#include <iostream>

/**
 * @brief OrderSystem 类的构造函数
 * @note 负责初始化高强度伪随机数生成器引擎（Mersenne Twister），利用硬件熵源提供高离散度的随机种子，
 * 从底层构筑仿真环境中订单流生成的随机分布特性。
 */
OrderSystem::OrderSystem()
    : rng(std::random_device{}()) {
}

/**
 * @brief 随机订单动态生成机制算法（具备时空解耦与自旋去重特性）
 * @param validShelfStationIds 当前仿真场景中所有合法可选的货架物理拓扑站点 ID 集合
 * @note 1. 执行前置安全性空检查；
 * 2. 引入自旋锁判定式去重机制，防止同一物理货架并发产生多个物料流转需求；
 * 3. 依据货架拓扑空间坐标区域，执行物料类型（SKU）的强约束硬绑定；
 * 4. 维护全局自增唯一订单主键。
 */
void OrderSystem::generateRandomOrder(const std::vector<int>& validShelfStationIds) {
    // ----------------------------------------------------------------
    // 1. 输入有效性安全拦截
    // ----------------------------------------------------------------
    if (validShelfStationIds.empty()) {
        return;
    }

    // 声明基于 C++11 标准的均匀分布算子，用于在合法货架集合范围内等概率抓取物理站点
    std::uniform_int_distribution<size_t> stationDist(0, validShelfStationIds.size() - 1);

    int chosenStationId = -1;
    bool isDuplicate = true;
    int attempts = 0; // 自旋深度保护计数器，防止在极端满载任务池下发生死循环

    // ----------------------------------------------------------------
    // 2. 自旋式多重任务空间互斥去重算法
    // ----------------------------------------------------------------
    while (isDuplicate && attempts < 15) {
        // 步骤 2.1：利用随机数引擎提取候选物理站点 ID
        chosenStationId = validShelfStationIds[stationDist(rng)];
        isDuplicate = false;
        attempts++;

        // 步骤 2.2：遍历全局活跃任务大盘（activeOrders），检查该货架是否已有未履行的订单
        for (const auto& existingOrder : activeOrders) {
            // 核心锁闭机制：若目标货架存在仍处于 WAITING（等待分配）或 PROCESSING（执行中）的任务，
            // 则判定触发资源占用冲突阻断，终止后续分配，准备执行下一轮自旋探测。
            if (existingOrder.targetStationId == chosenStationId) {
                isDuplicate = true;
                break;
            }
        }
    }

    // ----------------------------------------------------------------
    // 3. 边界资源耗尽软拦截防御
    // ----------------------------------------------------------------
    // 若达到最大自旋尝试深度（15次）仍未筛选出纯净空闲货架（说明当前大部分可用货架均在跑任务），
    // 算法执行防御性软拦截返回，放弃本帧生成，有效规避多线程任务主调度引擎出现时钟锁锁闭死锁风险。
    if (isDuplicate) {
        return;
    }

    // ----------------------------------------------------------------
    // 4. 订单实体装配与物料类别（SKU）空间映射强绑定
    // ----------------------------------------------------------------
    Order order;
    order.orderId = nextOrderId++; // 全局单调递增唯一订单主键注入
    order.targetStationId = chosenStationId;

    // 核心映射机制：依据物理货架在仓储大盘中的绝对拓扑区域划分，强解耦并赋作物料分类约束
    if ((chosenStationId >= 52 && chosenStationId <= 60) || (chosenStationId >= 63 && chosenStationId <= 96)) {
        // 区域类型 A：高热度橙色货架区 -> 刚性绑定 A 类物料（离散索引 0）
        order.sku = static_cast<SKUType>(0);
    }
    else if ((chosenStationId >= 1 && chosenStationId <= 6) ||
        (chosenStationId >= 13 && chosenStationId <= 16) ||
        (chosenStationId >= 25 && chosenStationId <= 30)) {
        // 区域类型 C：边缘低频灰色货架区 -> 刚性绑定 C 类物料（离散索引 2）
        order.sku = static_cast<SKUType>(2);
    }
    else {
        // 区域类型 B：标称标准蓝色货架区 -> 刚性绑定 B 类物料（离散索引 1）
        order.sku = static_cast<SKUType>(1);
    }

    // 步骤 4.1：将订单状态初始化标记为挂起待分配（WAITING）
    order.status = OrderStatus::WAITING;

    // 步骤 4.2：将装配完毕的订单推入活跃任务看板向量，正式交付调度系统
    activeOrders.push_back(order);
}

/**
 * @brief 检索全局系统任务池的活跃活跃度状态
 * @retval bool 若当前任务大盘包含任意未完结订单返回 true，否则返回 false
 */
bool OrderSystem::hasOrders() const {
    return !activeOrders.empty();
}

/**
 * @brief 轮询检索首个处于就绪态的等待派发订单指针
 * @retval Order* 指向最早生成的待分配订单实体内存指针；若无任何就绪订单，安全返回 nullptr
 * @note 遵循经典 FIFO（先进先出）业务逻辑。WMS 中央核心在每个离散控制 Tick 中通过此函数提取队首任务，
 * 作为全局分发给空闲 AGV 智能体的决策基准。
 */
Order* OrderSystem::getFirstWaitingOrder() {
    for (auto& o : activeOrders) {
        // 逐序扫描，锁定首个满足 WAITING 准入条件的订单实体
        if (o.status == OrderStatus::WAITING) {
            return &o;
        }
    }
    return nullptr;
}

/**
 * @brief 订单生命周期完结销毁与内存清退接口
 * @param orderId 被外部 WarehouseManager 宣告完结的订单唯一标识 ID
 * @note 当执行任务的 AGV 小车成功搬运载荷至出货口（OUT站）并完成规定的物理接驳锁闭后，
 * 调用此接口通过迭代器定位目标，将该订单从活跃向量中进行物理擦除，释放相应的堆内存碎片。
 */
void OrderSystem::completeOrder(int orderId) {
    for (auto it = activeOrders.begin(); it != activeOrders.end(); ++it) {
        if (it->orderId == orderId) {
            activeOrders.erase(it); // 物理注销，缩减活跃看板任务开销
            break;
        }
    }
}

/**
 * @brief 人工干预/鼠标交互定向订单实时催生接口
 * @param targetStationId 外部 GUI 鼠标点击事件触发的货架目标物理站点 ID
 * @param sku 用户指定强制派发的物料 SKU 类别
 * @retval Order* 返回在动态活跃向量中新生成的订单实体内存指针
 * @note 绕过系统标准随机分布流，专门提供给前端交互层用于系统极限压测或特定货架强行呼叫物料的最高优先级通路。
 */
Order* OrderSystem::createNewOrder(int targetStationId, SKUType sku) {
    Order order;
    order.orderId = nextOrderId++;            // 唯一识别号原子增
    order.targetStationId = targetStationId;  // 强行绑定交互层指定的货架站点
    order.sku = sku;                          // 写入特定的货物类型
    order.status = OrderStatus::WAITING;      // 注入初始状态状态机为挂起分配

    // 压入系统活跃链表中维护
    activeOrders.push_back(order);

    // 步骤 1.1：通过获取向量尾部引用的底层内存地址，将其作为长效指针回传给上层控制逻辑
    return &activeOrders.back();
}

/**
 * @brief 精确的主键 ID 订单实体检索检索器
 * @param orderId 待查询的全局唯一订单标识主键
 * @retval Order* 返回指向目标订单实体的动态内存指针；若全局查无此单，安全返回 nullptr
 */
Order* OrderSystem::getOrderById(int orderId) {
    for (auto& o : activeOrders) {
        if (o.orderId == orderId) {
            return &o; // 命中主键，返回其实体物理内存地址
        }
    }
    return nullptr; // 执行边界防护，杜绝野指针悬挂
}

/**
 * @brief 全局时钟周期内订单饥饿度指标单向自增累加器
 * @note 在每帧系统时钟向前演进时，针对大盘中仍处于 WAITING 挂起排队状态的所有订单，
 * 将其内部的 waitTicks 计数器执行单向单步自增。该时钟生命周期数据专门用以支撑后续
 * 基于运筹学匈牙利算法（Hungarian Optimization）调度中的饥饿度权重补偿与效能反哺。
 */
void OrderSystem::updateOrderTicks() {
    for (auto& o : activeOrders) {
        if (o.status == OrderStatus::WAITING) {
            o.waitTicks++; // 处于调度排队等待队列中的订单生存周期系统时钟计数器单向自增
        }
    }
}