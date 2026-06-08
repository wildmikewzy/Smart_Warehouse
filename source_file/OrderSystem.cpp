#include "OrderSystem.h"
#include <iostream>
/**
 * @brief OrderSystem 的构造函数
 * @param 无
 * @retval 无
 * @details 初始化随机数生成器（Mersenne Twister），利用硬件随机种子确保每次运行仿真时订单流的随机性。
 */
OrderSystem::OrderSystem()
    : rng(std::random_device{}()) {
}

/**
 * @brief 在系统中随机生成一个包含不同 SKU 类型和目标货架网格的业务订单（防重复改良版）
 * @param validShelfStationIds 仓库中当前所有合法可选的货架站点（Station ID）集合列表
 * @retval 无
 * @details
 * 1. 首先对输入的货架站点列表进行安全性空检查，若无合法货架则直接拦截返回。\n
 * 2. 利用均匀分布随机抽取一个货架，并结合当前活跃队列进行动态去重，确保同货架不并发。\n
 * 3. 利用离散权重分布（按 60% A类、30% B类、10% C类的高级仓储热度规律）为订单赋予物料 SKU 属性。\n
 * 4. 自动递增生成唯一的订单 ID，将订单初始状态标记为 WAITING（等待分配），最后压入活跃订单队列。
 */
void OrderSystem::generateRandomOrder(const std::vector<int>& validShelfStationIds) {
    if (validShelfStationIds.empty()) {
        return;
    }

    std::uniform_int_distribution<size_t> stationDist(0, validShelfStationIds.size() - 1);      // 均匀分布随机选择一个货架站点
    std::discrete_distribution<int> skuDist({ 60, 30, 10 });        // 离散分布随机选择 SKU 类型，权重分别为 60% A类、30% B类、10% C类

    int chosenStationId = -1;
    bool isDuplicate = true;
    int attempts = 0; // 防死循环计数器（若仓储货架全部处于有单状态，安全退出）

    // 🌟【硬核自旋去重】：完美保护你的标准随机分布
    while (isDuplicate && attempts < 15) {
        // 使用你原本优雅的 C++11 标准分布进行随机抓取
        chosenStationId = validShelfStationIds[stationDist(rng)];
        isDuplicate = false;
        attempts++;

        // 核心检查：如果这个货架已经存在于活跃订单列表（activeOrders）中，则判定为冲突
        for (const auto& existingOrder : activeOrders) {
            // 只要订单还没被彻底消费完（处于 WAITING 或 PROCESSING 状态），就不能重复去同一个货架
            if (existingOrder.targetStationId == chosenStationId) {
                isDuplicate = true;
                break; // 命中雷区，直接打破当前内循环，准备进入下一轮自旋随机
            }
        }
    }

    // 防御保护：如果随机了15次都撞车（说明当前大部分可用货架都在跑任务呢），
    // 这一帧就放弃生成，把宝贵的系统带宽留给正在跑的小车，绝对不卡死主线程。
    if (isDuplicate) {
        return;
    }

    // 走到这里，说明 chosenStationId 纯净、唯一、且完美符合热度分布！
    Order order;
    order.orderId = nextOrderId++;
    order.targetStationId = chosenStationId;
    order.sku = static_cast<SKUType>(skuDist(rng));
    order.status = OrderStatus::WAITING;

    activeOrders.push_back(order);
}

/**
 * @brief 检查当前系统任务池中是否存在待处理或正在执行的活跃订单
 * @param 无
 * @retval bool 如果订单池不为空返回 true，否则返回 false
 */
bool OrderSystem::hasOrders() const {
    return !activeOrders.empty();
}

/**
 * @brief 从活跃订单池中检索首个处于“等待派发”状态的订单源指针
 * @param 无
 * @retval Order* 指向首个未分配任务订单对象的指针；若当前没有等待中的订单，则返回 nullptr
 * @details WMS 调度大脑在每帧轮询时会调用此接口。它采用 FIFO（先进先出）原则，遍历订单池，
 * 挑出最早生成的、状态仍为 OrderStatus::WAITING 的订单并返回，供空闲 AGV 锁定执行。
 */
Order* OrderSystem::getFirstWaitingOrder() {
    for (auto& o : activeOrders) {
        if (o.status == OrderStatus::WAITING) {
            return &o; // 找到第一个处于等待派发的订单
        }
    }
    return nullptr;
}

/**
 * @brief 完结指定 ID 的业务订单，将其从当前的活跃任务看板/队列中销毁注销
 * @param orderId 需要执行完结结算的订单唯一标识 ID
 * @retval 无
 * @details 当 AGV 小车成功携货抵达出货口（OUT站）并完成 3 秒的卸货锁定后，
 * WarehouseManager 会触发此函数。通过迭代器匹配目标 ID，将该订单从活跃向量 `activeOrders` 中物理移除，释放内存。
 */
void OrderSystem::completeOrder(int orderId) {
    for (auto it = activeOrders.begin(); it != activeOrders.end(); ++it) {
        if (it->orderId == orderId) {
            activeOrders.erase(it); // 销毁已完结订单
            break;
        }
    }
}
/**
 * @brief 由手动鼠标点击触发，定向为特定货架生成一张新业务订单
 */
Order* OrderSystem::createNewOrder(int targetStationId, SKUType sku) {
    Order order;
    order.orderId = nextOrderId++;          // 唯一订单号自增
    order.targetStationId = targetStationId; // 强行绑定鼠标点击的货架
    order.sku = sku;                        // 赋予传入的物料类型
    order.status = OrderStatus::WAITING;    // 初始标记为等待分配

    // 压入活跃订单链表中
    activeOrders.push_back(order);

    // 顺着链表尾部，直接把刚刚生出来的订单在内存中的真实地址（指针）送出去返回
    return &activeOrders.back();
}
/**
 * @brief 依据订单 ID 查找并返回订单对象的肉体指针
 */
Order* OrderSystem::getOrderById(int orderId) {
    for (auto& o : activeOrders) {
        if (o.orderId == orderId) {
            return &o; // 找到则返回订单肉体指针
        }
    }
    return nullptr; // 防御保护，没找到安全返回空
}
/**
 * @brief  每帧更新等待中订单的饥饿计数器
 */
void OrderSystem::updateOrderTicks() {
    for (auto& o : activeOrders) {
        if (o.status == OrderStatus::WAITING) {
            o.waitTicks++; // 处于等待队列中的订单痛苦指数随时间递增
        }
    }
}