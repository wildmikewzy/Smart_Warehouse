#pragma once
#include "common.h"
#include "Robot.h"
#include "Map.h"
#include "Router.h"
#include <vector>
#include <map> // 【新增】用于存储和快速查询站点映射表
#include <utility>

using namespace std;

// 【新增方案三核心数据结构】拓扑站点
struct Station {
    int stationId;      // 站点唯一ID
    Point dockPoint;    // 机器人实际寻路停靠点（可通行的空地）
    Point shelfPoint;   // 该站点对应的实体货架位置（不可通行的障碍物）
};

class WarehouseManager {
private:
    vector<Robot> robots;               // 管理的机器人列表
    Map warehouseMap;                   // 仓库的地图数据
    ReservationTable globalTable;       // 全局时空预定表，用于规避多车抢点
    int systemTick;                     // 全局系统时间戳（逻辑步数）

    // 【新增】仓库内所有解耦站点的集合，Key为stationId
    std::map<int, Station> stations;
    int gridToStationCache[20][20]; // 快速空间检索缓存，存储 stationId。无站点则为 -1

public:
    WarehouseManager();                 // 仓库构造函数
    void setupScene();                  // 初始化：放置机器人、货架并生成解耦站点

    // 【接口重构】传入机器人ID和业务层指定的站点ID
    void dispatchRobot(int robotId, int stationId);

    void updateAll();                   // 每帧逻辑更新

    // 【Stress Test】重置机器人和全局时空表（用于极端测试）
    void prepareStressTest(const std::vector<std::pair<int, Point>>& placements);

    // 供 GUI 调用，获取数据进行绘制
    const vector<Robot>& getRobots() const { return robots; }
    const Map& getMap() const { return warehouseMap; }

    // 供GUI或外界查询当前的拓扑站点信息
    const std::map<int, Station>& getStations() const { return stations; }
    int getStationIdByGrid(int x, int y) const {
        if (x >= 0 && x < 20 && y >= 0 && y < 20) {
            return gridToStationCache[x][y];
        }
        return -1;
    }
};