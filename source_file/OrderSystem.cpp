#include "OrderSystem.h"

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
 * @brief 在系统中随机生成一个包含不同 SKU 类型和目标货架网格的业务订单
 * @param validShelfStationIds 仓库中当前所有合法可选的货架站点（Station ID）集合列表
 * @retval 无
 * @details
 * 1. 首先对输入的货架站点列表进行安全性空检查，若无合法货架则直接拦截返回。\n
 * 2. 利用均匀分布随机抽取一个货架作为本次任务的取货站点。\n
 * 3. 利用离散权重分布（按 60% A类、30% B类、10% C类的高级仓储热度规律）为订单赋予物料 SKU 属性。\n
 * 4. 自动递增生成唯一的订单 ID，将订单初始状态标记为 WAITING（等待分配），最后压入活跃订单队列。
 */
void OrderSystem::generateRandomOrder(const std::vector<int>& validShelfStationIds) {
    if (validShelfStationIds.empty()) {
        return;
    }

	std::uniform_int_distribution<size_t> stationDist(0, validShelfStationIds.size() - 1);      // 均匀分布随机选择一个货架站点
	std::discrete_distribution<int> skuDist({ 60, 30, 10 });        // 离散分布随机选择 SKU 类型，权重分别为 60% A类、30% B类、10% C类

    Order order;
    order.orderId = nextOrderId++;
    order.targetStationId = validShelfStationIds[static_cast<int>(stationDist(rng))];
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