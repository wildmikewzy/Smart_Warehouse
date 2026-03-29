#include "WarehouseManager.h"

WarehouseManager::WarehouseManager() {
    setupScene();
}
void WarehouseManager::setupScene() {
    // 放置一排货架 (类型1)
    for (int i = 5; i < 15; i++) {
        warehouseMap.setObstacle(i, 10, 1);
    }

    // 放置一个出货点 (类型2)
    warehouseMap.setObstacle(15, 15, 2);

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
            break;
        }
    }
}

void WarehouseManager::updateAll() {        //更新所有机器人的状态，这个函数需要在主循环里每帧调用一次
    for (auto& r : robots) {
        r.update();
    }
}