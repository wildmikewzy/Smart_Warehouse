#pragma once
#include <list>
#include <vector>
#include <random>
#include "common.h"

enum class OrderStatus { WAITING, PROCESSING, FINISHED };

struct Order {  
    int orderId;
    int targetStationId;
    SKUType sku;
    OrderStatus status;
};

class OrderSystem {
private:
    std::list<Order> activeOrders; // 改用 list 以支持并发读取与任意删除
    int nextOrderId = 1;
    std::mt19937 rng;

public:
    OrderSystem();

    void generateRandomOrder(const std::vector<int>& validShelfStationIds);
    bool hasOrders() const;
    Order* getFirstWaitingOrder();     // 新增：返回第一个等待业务的引用
    void completeOrder(int orderId);   // 新增：根据 ID 精准核销订单

    // 获取当前所有订单快照以供 GUI 渲染
    std::vector<Order> getAllOrders() const {
        std::vector<Order> orders;
        for (const auto& o : activeOrders) {
            orders.push_back(o);
        }
        return orders;
    }
};