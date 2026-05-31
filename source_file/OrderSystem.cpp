#include "OrderSystem.h"

OrderSystem::OrderSystem()
    : rng(std::random_device{}()) {
}

void OrderSystem::generateRandomOrder(const std::vector<int>& validShelfStationIds) {
    if (validShelfStationIds.empty()) {
        return;
    }

    std::uniform_int_distribution<size_t> stationDist(0, validShelfStationIds.size() - 1);
    std::discrete_distribution<int> skuDist({ 60, 30, 10 });

    Order order;
    order.orderId = nextOrderId++;
    order.targetStationId = validShelfStationIds[static_cast<int>(stationDist(rng))];
    order.sku = static_cast<SKUType>(skuDist(rng));
    order.status = OrderStatus::WAITING;

    activeOrders.push_back(order);
}

bool OrderSystem::hasOrders() const {
    return !activeOrders.empty();
}

Order* OrderSystem::getFirstWaitingOrder() {
    for (auto& o : activeOrders) {
        if (o.status == OrderStatus::WAITING) {
            return &o; // 找到第一个处于等待派发的订单
        }
    }
    return nullptr;
}

void OrderSystem::completeOrder(int orderId) {
    for (auto it = activeOrders.begin(); it != activeOrders.end(); ++it) {
        if (it->orderId == orderId) {
            activeOrders.erase(it); // 销毁已完结订单
            break;
        }
    }
}