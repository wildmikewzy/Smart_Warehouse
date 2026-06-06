#pragma once
#include <list>
#include <vector>
#include <random>
#include "common.h"

enum class OrderStatus { WAITING, PROCESSING, FINISHED };

struct Order {  
    int orderId;        //订单编号
	int targetStationId;        //目标货架站点 ID
	SKUType sku;        //订单物料 SKU 类型
	OrderStatus status;     //订单状态：等待派发、正在处理、已完成
    int waitTicks = 0;      // 订单等待更新的帧数计数器
    int createTick = 0; // 订单创建时的系统时钟（Tick数）
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
    Order* getFirstWaitingOrder();     // 返回第一个等待业务的引用
    void completeOrder(int orderId);   // 根据 ID 精准核销订单
	Order* getOrderById(int orderId); // 根据订单 ID 获取订单指针，供调度系统查询订单状态
    Order* createNewOrder(int targetStationId,SKUType sku);    // 鼠标点击货架时，动态定向生成新订单，并返回该新订单的肉体指针
    void updateOrderTicks(); // 每帧更新等待中订单的饥饿计数器
    // 获取当前所有订单快照
    std::vector<Order> getAllOrders() const {
        std::vector<Order> orders;
        for (const auto& o : activeOrders) {
            orders.push_back(o);
        }
        return orders;
    }
    // 供 Manager 遍历修改订单状态用
    std::list<Order>& getNonConstActiveOrders() { return activeOrders; }
    // 【测试用】：一键清空当前任务池（确保测试开始时大盘纯净）
    void clearAllOrders() {
        activeOrders.clear();
    }

    // 【测试用】：刚性注入一个确定属性的测试订单
    void injectCustomOrder(int stationId, int skuTypeInt) {
        Order order;
        order.orderId = nextOrderId++;
        order.targetStationId = stationId;
        order.sku = static_cast<SKUType>(skuTypeInt); // 强转为你 common.h 中的 SKUType
        order.status = OrderStatus::WAITING;
        order.waitTicks = 0; // 初始痛苦指数为0
        activeOrders.push_back(order);
    }
};