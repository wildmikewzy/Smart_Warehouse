/*仓库业务调度中枢*/
#pragma once
#include "common.h"
#include "Robot.h"
#include "Map.h"
#include "Router.h"
#include <vector>
using namespace std;

class WarehouseManager {
private:
    vector<Robot> robots;      //管理的机器人列表
    Map warehouseMap;           //仓库的地图数据

public:
    WarehouseManager();     //仓库构造函数
    void setupScene();  // 初始化：放置机器人和货架
    void dispatchRobot(int robotId, Point targetGrid);// 核心业务：给指定 ID 的机器人下达去某个点的指令
    void updateAll();       // 每帧逻辑更新

    // 供 GUI 调用，获取数据进行绘制
    const vector<Robot>& getRobots() const { return robots; }      //获取机器人列表
    const Map& getMap() const { return warehouseMap; }      //获取地图数据
};