#include "WarehouseManager.h"
#include "Map."
#include <iostream>
#include <vector>
/*
* @brief 仓库初始化，仓库构造函数，负责设置初始场景，包括放置货架和机器人
**/
WarehouseManager::WarehouseManager() {
    setupScene();
}
void WarehouseManager::setupScene() {
    // 首先清空可能存在的旧货架设置（假设默认地图初始化为全0空地）
    
    // 象限货架簇布置
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            // 1. 最外围一圈作为安全回转道，保持空地
            if (x == 0 || x == 19 || y == 0 || y == 19) {
                continue;
            }
            // 2. 十字主通道，保持中间行列(9和10)为空地
            if (x == 9 || x == 10 || y == 9 || y == 10) {
                continue;
            }

            // 3. 左上象限：X 范围 [2, 3] 和 [6, 7]；Y 范围 [2, 7]
            if (((x >= 2 && x <= 3) || (x >= 6 && x <= 7)) && (y >= 2 && y <= 7)) {
                warehouseMap.setObstacle(x, y, 1);
            }
            // 4. 左下象限：X 范围 [2, 3] 和 [6, 7]；Y 范围 [12, 17]
            else if (((x >= 2 && x <= 3) || (x >= 6 && x <= 7)) && (y >= 12 && y <= 17)) {
                warehouseMap.setObstacle(x, y, 1);
            }
            // 5. 右上象限：X 范围 [12, 13] 和 [16, 17]；Y 范围 [2, 7]
            else if (((x >= 12 && x <= 13) || (x >= 16 && x <= 17)) && (y >= 2 && y <= 7)) {
                warehouseMap.setObstacle(x, y, 1);
            }
            // 6. 右下象限：X 范围 [12, 13] 和 [16, 17]；Y 范围 [12, 17]
            else if (((x >= 12 && x <= 13) || (x >= 16 && x <= 17)) && (y >= 12 && y <= 17)) {
                warehouseMap.setObstacle(x, y, 1);
            }
        }
    }

    // 放置一个出货点 (类型2)，放置在适当位置，你可以根据需要调整
    warehouseMap.setObstacle(19, 10, 2);

    // 初始化机器人位置
    robots.push_back(Robot(1, { 0, 10 }));
    robots.push_back(Robot(2, { 0, 9 }));
}
/**
* @brief 调度机器人函数
* @param robotId 需要调度的机器人ID
* @param targetGrid 目标格点坐标
* 
*/
void WarehouseManager::dispatchRobot(int robotId, Point targetGrid) {     //给指定ID的机器人下达去到某个点的指令
	//基础合法性检查，确保选中的机器人是存在的，且目标点在地图范围内
    if(robotId < 1 || robotId > robots.size()) {
        cout << "错误：机器人ID " << robotId << " 不存在！" << endl;
        return;
	}
    if (targetGrid.x < 0 || targetGrid.x >= MAP_LENGTH || targetGrid.y < 0 || targetGrid.y >= MAP_WIDTH) {
        cout << "错误：目标点 (" << targetGrid.x << ", " << targetGrid.y << ") 超出地图范围！" << endl;
        return;
    }
	 
    //目标点占用检查：如果目标点被空闲状态机器人占用，则拒绝指令
    for (const auto& r : robots) {
        if (std::abs(r.currentPos.x - targetGrid.x) < 0.1 &&  std::abs(r.currentPos.y - targetGrid.y) < 0.1 && r.status == RobotStatus::IDLE) {
            cout << "错误：目标点 (" << targetGrid.x << ", " << targetGrid.y << ") 已被机器人 " << r.id << " 占用！" << endl;
            return;
		}
    }
    //探测该目标在真实地图里是不是本身就是不可通行的障碍物(货架)
    bool isTargetObstacle = !warehouseMap.isWalkable(targetGrid.x, targetGrid.y);

    Map shadowMap = this->warehouseMap; // 创建地图的影子
    //将所有“非当前选中的机器人设定为不可以通行”
    for (const auto& robot : robots) {
		if (robot.id == robotId) continue;     //当前选中的机器人保持原状
		//其他机器人的位置在影子地图上设置为货架，把它当成障碍物
		shadowMap.setObstacle(robot.currentPos.x, robot.currentPos.y, 1);
    }
    
    // 为了让路由算法能够搜索到该目标点位置，我们在影子地图中临时将终点设置为空地，修改是临时的，不会影响真实的地图数据
    shadowMap.setObstacle(targetGrid.x, targetGrid.y, 0);

    for (auto& r : robots) {
        if (r.id == robotId) {
            // 传入第四个参数 isTargetObstacle 让寻路算法得知是否需要剔除尾部
			std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, shadowMap, isTargetObstacle);    
            r.setPath(path);
            //如果返回的路径点向量为空，说明没找到路径
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

void WarehouseManager::updateAll() {        //更新所有机器人的状态，在主循环中调用
    for (auto& r : robots) {
        r.update();
    }
}
