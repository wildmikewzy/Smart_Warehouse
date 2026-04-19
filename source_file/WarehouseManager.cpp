#include "WarehouseManager.h"
#include <iostream>
/*
* @brief 仓库初始化
**/
WarehouseManager::WarehouseManager() {
    setupScene();
}
void WarehouseManager::setupScene() {
    // 放置一排货架 (类型1)
    for (int i = 5; i < 15; i++) {
        warehouseMap.setObstacle(float(i), 10, 1);
    }

    // 放置一个出货点 (类型2)
    warehouseMap.setObstacle(15, 15, 2);
    //设置障碍物
    warehouseMap.setObstacle(5, 5, 1);
    warehouseMap.setObstacle(5, 6, 1);
	warehouseMap.setObstacle(4, 9, 1);
    warehouseMap.setObstacle(5, 14, 1);
    warehouseMap.setObstacle(5, 15, 1);
    // 初始化机器人位置
    robots.push_back(Robot(1, { 2, 2 }));
    robots.push_back(Robot(2, { 2, 17 }));
}

void WarehouseManager::dispatchRobot(int robotId, Point targetGrid) {       //给指定ID的机器人下达去到某个点的指令
    for (auto& r : robots) {
        if (r.id == robotId) {
            // 调用 Router 获取路径
            std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, warehouseMap);
            r.setPath(path);

            // 在控制台输出路径坐标
            cout << "机器人 " << robotId << " 的路径: ";
            for (const auto& p : path) {
                cout << "(" << p.x << "," << p.y << ") ";
            }
            cout << std::endl;

            break;
        }
    }
}

void WarehouseManager::updateAll() {        //更新所有机器人的状态，这个函数需要在主循环里每帧调用一次
    for (auto& r : robots) {
        r.update();
    }
}