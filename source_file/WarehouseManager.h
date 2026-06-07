#pragma once
#include "common.h"
#include "Robot.h"
#include "Map.h"
#include "Router.h"
#include "OrderSystem.h" // 引入订单大脑
#include <vector>
#include <map>
#include <utility>
class GUI;      // 前向声明，避免循环依赖
using namespace std;

class WarehouseManager {
private:
    //vector<Robot> robots;               // 管理的机器人列表
    Map warehouseMap;                   // 仓库的地图数据
    ReservationTable globalTable;       // 全局时空预定表，用于规避多车抢点
    int systemTick;                     // 全局系统时间戳（逻辑步数)

    // 全新成员变量
    OrderSystem orderSystem;            // 引入订单大脑
    std::map<int, Station> stations;    // 管理所有的货架拓扑站点
    int gridToStationCache[20][20];     // 空间坐标到站点 ID 的快速检索表
    // 🌟 新增：全局效能统计大盘
    int globalSystemTick = 0;        // 全局系统时钟（每帧++）
    int totalCompletedOrders = 0;    // 系统总共完成的订单数
    long long totalOrderDuration = 0; // 所有已完成订单的生存周期Tick总和
    // 🌟 新增：压测弹窗专用的状态控制旗帜
    bool isStressTesting = false;      // 当前是否正处于 P 键压测过程中
    bool hasReportedMetrics = false;   // 本轮压测是否已经弹出过报告

public:
    vector<Robot> robots;               // 管理的机器人列表
    WarehouseManager();                 // 仓库构造函数
    void setupScene();                  // 初始化：放置机器人、货架并生成解耦站点

    // 传入机器人ID和业务层指定的站点ID
    void dispatchRobot(int robotId, int stationId);

    void updateAll(GUI & gui);                   // 每帧逻辑更新

    // 重置机器人和全局时空表（用于极端测试）
    void prepareStressTest(const std::vector<std::pair<int, Point>>& placements);

    // 基础接口（确保可用性）
    const vector<Robot>& getRobots() const { return robots; }
    const Map& getMap() const { return warehouseMap; }

    // 新增：暴露订单大脑的常量引用供 GUI 使用
    const OrderSystem& getOrderSystem() const { return orderSystem; }
    OrderSystem& getOrderSystem() { return orderSystem; }   //非 const 版本（供鼠标点击、动态印单等需要修改数据的场景使用）
	void dispatchOrdersByHungarian(); // 基于匈牙利算法的全局集中式调度核心
    // 全新的快速查询接口
    const std::map<int, Station>& getStations() const { return stations; }
    int getStationIdByGrid(int x, int y) const {
        if (x >= 0 && x < 20 && y >= 0 && y < 20) {
            return gridToStationCache[x][y];
        }
        return -1;
    }
    Station getStationById(int stationId) {
        return stations[stationId];
    }
    // 🌟 新增：打包当前所有核心指标的结构体，方便后续弹窗直接读取
    struct LogisticsMetrics {
        double emptyRunningRate;     // 空驶率 (%)
        double avgOrderCompletionTime;// 平均订单完成耗时 (Ticks/单)
        double throughputPerKGrid;   // 单位时间吞吐量 (单/1000 Ticks)
    };

    void update();
    void onOrderCreated(Order& order);
    void onOrderFinished(const Order& order);
    LogisticsMetrics calculateMetrics();
    void moveRobotToGrid(int robotId, int targetX, int targetY);
};