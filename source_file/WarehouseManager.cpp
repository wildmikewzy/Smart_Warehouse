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
    warehouseMap.setObstacle(2, 16, 1);
    warehouseMap.setObstacle(1, 17, 1);
    warehouseMap.setObstacle(2, 18, 1);
    warehouseMap.setObstacle(3, 17, 1);
    // 初始化机器人位置
    robots.push_back(Robot(1, { 2, 2 }));
    robots.push_back(Robot(2, { 2, 17 }));
    robots.push_back(Robot(3, { 5, 8 }));
	robots.push_back(Robot(4, { 6, 9 }));
	robots.push_back(Robot(5, { 5, 7 }));
}
/**
* @brief 调度机器人函数
* @param robotId 需要调度的机器人ID
* @param targetGrid 目标格点坐标
* 
*/
void WarehouseManager::dispatchRobot(int robotId, Point targetGrid) {     //给指定ID的机器人下达去到某个点的指令
	//==基础合法性检查，确保选中的机器人是存在的，且目标点在地图范围内==
    if(robotId < 1 || robotId > robots.size()) {
        cout << "错误：机器人ID " << robotId << " 不存在！" << endl;
        return;
	}
    if (targetGrid.x < 0 || targetGrid.x >= MAP_LENGTH || targetGrid.y < 0 || targetGrid.y >= MAP_WIDTH) {
        cout << "错误：目标点 (" << targetGrid.x << ", " << targetGrid.y << ") 超出地图范围！" << endl;
        return;
    }
	 
    //===目标点占用检查：如果目标点被障碍物或者空闲状态机器人占用，则拒绝指令===
    for (const auto& r : robots) {
        if (std::abs(r.currentPos.x - targetGrid.x) < 0.1 &&  std::abs(r.currentPos.y - targetGrid.y) < 0.1 && r.status == RobotStatus::IDLE) {
            cout << "错误：目标点 (" << targetGrid.x << ", " << targetGrid.y << ") 已被机器人 " << r.id << " 占用！" << endl;
            return;
		}
    }
    if(!warehouseMap.isWalkable(targetGrid.x, targetGrid.y)) {
        cout << "错误：目标点 (" << targetGrid.x << ", " << targetGrid.y << ") 是障碍物！" << endl;
        return;
	}
    Map shadowMap = this->warehouseMap; // 创建地图的影子
    //将所有“非当前选中的机器人设定为不可以通行”
    for (const auto& robot : robots) {
		if (robot.id == robotId) continue;     //当前选中的机器人保持原状
		//将其他机器人的位置在影子地图上设置为障碍物（类型1）
		shadowMap.setObstacle(robot.currentPos.x+0.5 ,robot.currentPos.y+0.5, 1);
    }
    for (auto& r : robots) {
        if (r.id == robotId) {
            // 调用 Router 获取路径
			std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, shadowMap);     //使用将机器人点位标记成不可通行的影子地图进行路径规划，确保路径不会穿过其他机器人
            r.setPath(path);
            if (path.empty()) {
				cout << "警告：机器人 " << robotId << " 无法找到路径到达目标点 (" << targetGrid.x << ", " << targetGrid.y << ")！" << endl;
            }
            else{
                // 在控制台输出路径坐标
                cout << "机器人 " << robotId << " 的路径: ";
                for (const auto& p : path) {
                    cout << "(" << p.x << "," << p.y << ") ";
                }
                cout << std::endl;
            }
            

            break;
        }
    }
}

void WarehouseManager::updateAll() {        //更新所有机器人的状态，这个函数需要在主循环里每帧调用一次
    for (auto& r : robots) {
        r.update();
    }
}