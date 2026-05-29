#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include "WarehouseManager.h"

/**
* @brief 仓库初始化，仓库构造函数，负责设置初始场景，包括放置货架和机器人
**/
WarehouseManager::WarehouseManager() {
    systemTick = 0; // 初始化全局时钟
    setupScene();
}

void WarehouseManager::setupScene() {
    // 1. 初始化缓存为 -1
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            gridToStationCache[x][y] = -1;
        }
    }
    int currentStationId = 1; // 站点自增ID计数器

    // 2. 自动化部署货架与精准靠泊点映射
    // 规则协议：
    // - 左上象限：X 范围 [2,3] 和 [6,7]，Y 范围 [2,7]。
    //   取货靠泊点规则：若 X == 2 或 X == 6（货架左列），取货点在左侧（X - 1）；若 X == 3 或 X == 7（货架右列），取货点在右侧（X + 1）。
    // - 左下象限：X 范围 [2,3] 和 [6,7]，Y 范围 [12,17]。
    //   取货靠泊点规则：与左上象限一致，即沿 X 轴向外侧通道靠泊（X 为偶数向左，奇数向右）。
    // - 右上象限：X 范围 [12,13] 和 [16,17]，Y 范围 [2,7]。
    //   取货靠泊点规则：沿 X 轴向通道靠泊（X 为偶数向左，奇数向右）。
    // - 右下象限：X 范围 [12,13] 和 [16,17]，Y 范围 [12,17]。
    //   取货靠泊点规则：沿 X 轴向通道靠泊（X 为偶数向左，奇数向右）。
    // 请根据上述规则，用一个 0~19 的双重 for 循环，排除外围(0,19)和十字主通道(9,10)，
    // 建立静态障碍物，计算 Station 结构体，并将 shelfPoint 和 dockPoint 录入 stations 和 gridToStationCache 中。
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            if (x == 0 || x == 19 || y == 0 || y == 19) {
                continue;
            }
            if (x == 9 || x == 10 || y == 9 || y == 10) {
                continue;
            }

            bool inLeftCols = (x >= 2 && x <= 3) || (x >= 6 && x <= 7);
            bool inRightCols = (x >= 12 && x <= 13) || (x >= 16 && x <= 17);
            bool inTopRows = (y >= 2 && y <= 7);
            bool inBottomRows = (y >= 12 && y <= 17);

            if (!(inLeftCols || inRightCols) || !(inTopRows || inBottomRows)) {
                continue;
            }

            warehouseMap.setObstacle(x, y, 1);

            Station station;
            station.stationId = currentStationId++;
            station.shelfPoint = { x, y };

            int dockX = (x % 2 == 0) ? (x - 1) : (x + 1);
            station.dockPoint = { dockX, y };

            stations[station.stationId] = station;

            gridToStationCache[x][y] = station.stationId;
            if (dockX >= 0 && dockX < 20) {
                gridToStationCache[dockX][y] = station.stationId;
            }
        }
    }
    // 放置一个出货点，同样可以为其绑定一个出货站点（例如站号999）
    warehouseMap.setObstacle(19, 10, 2);
    Station outStation;
    outStation.stationId = 999;
    outStation.shelfPoint = { 19, 10 };
    outStation.dockPoint = { 18, 10 }; // 停靠在出货点左侧一格空地
    stations[outStation.stationId] = outStation;

    // 初始化机器人位置
    robots.push_back(Robot(1, { 0, 10 }));
    robots.push_back(Robot(2, { 0, 9 }));
}

void WarehouseManager::prepareStressTest(const std::vector<std::pair<int, Point>>& placements) {
    systemTick = 0;
    globalTable.vertexReservations.clear();
    globalTable.edgeReservations.clear();

    for (const auto& placement : placements) {
        bool exists = false;
        for (const auto& r : robots) {
            if (r.id == placement.first) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            robots.push_back(Robot(placement.first, placement.second));
        }
    }

    for (auto& r : robots) {
        r.status = RobotStatus::IDLE;
        r.pathQueue.clear();
        r.historyPath.clear();
        r.currentSpeed = 0.0f;
        r.moveCooldown = 0;
        r.currentStepFrames = 15;
    }

    for (const auto& placement : placements) {
        for (auto& r : robots) {
            if (r.id == placement.first) {
                r.currentPos = placement.second;
                r.realX = static_cast<float>(placement.second.x);
                r.realY = static_cast<float>(placement.second.y);
                break;
            }
        }
    }
}

/**
* @brief 重构后的调度机器人函数
*/
void WarehouseManager::dispatchRobot(int robotId, int stationId) {
    // 1. 基础合法性检查
    if (robotId < 1 || robotId > robots.size()) {
        cout << "错误：机器人ID " << robotId << " 不存在！" << endl;
        return;
    }
    // 检查站点ID是否存在于映射库中
    if (stations.find(stationId) == stations.end()) {
        cout << "错误：业务站点 ID " << stationId << " 未在拓扑网络中注册！" << endl;
        return;
    }

    // 通过站点ID提取解耦后的实际寻路终点（靠泊空地）
    Station targetStation = stations[stationId];
    Point targetGrid = targetStation.dockPoint;

    cout << "系统调配：机器人 " << robotId << " 接受指令前往业务站点 [" << stationId
        << "]。物理货架位置:(" << targetStation.shelfPoint.x << "," << targetStation.shelfPoint.y
        << ") -> 时空寻路真实靠泊点:(" << targetGrid.x << "," << targetGrid.y << ")" << endl;

    // 2. 【预定表清理】擦除该机器人原有的所有时空点预定
    for (auto it = globalTable.vertexReservations.begin(); it != globalTable.vertexReservations.end(); ) {
        if (it->second == robotId) {
            it = globalTable.vertexReservations.erase(it);
        }
        else {
            ++it;
        }
    }
    globalTable.edgeReservations.clear();

    // 3. 【时空寻路】由于 targetGrid 已经是解耦出的可行走空地，无需再通过 shadowMap 强刷货架状态！
    // stopAdjacent 传 false，因为小车需要完完全全踏入这个停靠站点
    bool stopAdjacent = false;

    for (auto& r : robots) {
        if (r.id == robotId) {
            // 调用带有时间维度的 Router::getPath
            std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, warehouseMap, globalTable, robotId, systemTick, stopAdjacent);

            r.setPath(path);

            if (path.empty()) {
                cout << "警告：机器人 " << robotId << " 无法找到无冲突时空路径前往站点停靠点 (" << targetGrid.x << ", " << targetGrid.y << ")！" << endl;
            }
            else {
                // 4. 【时空锁定】将规划成功的路径锁进预定表
                for (size_t i = 0; i < path.size(); ++i) {
                    int t = systemTick + i;
                    globalTable.vertexReservations[{path[i].x, path[i].y, t}] = robotId;

                    if (i > 0) {
                        std::string edgeKey = to_string(path[i - 1].x) + "," + to_string(path[i - 1].y) + "->" +
                            to_string(path[i].x) + "," + to_string(path[i].y) + "@" + to_string(t);
                        globalTable.edgeReservations.insert(edgeKey);
                    }
                }

                // 【终点静止占领】到达站点后，原地持续锁定（防止别的车从小车身体里穿过去）
                Point finalPos = path.back();
                int arrivalTime = systemTick + path.size() - 1;
                for (int futureT = arrivalTime + 1; futureT < arrivalTime + 100; ++futureT) {
                    globalTable.vertexReservations[{finalPos.x, finalPos.y, futureT}] = robotId;
                }

                // 打印最终生成的完美路径
                cout << "机器人 " << robotId << " 时空闭环路径规划成功: ";
                for (const auto& p : path) {
                    cout << "(" << p.x << "," << p.y << ") ";
                }
                cout << std::endl;
            }
            break;
        }
    }
}

void WarehouseManager::updateAll() {
    bool anyRobotMoved = false;
    for (auto& r : robots) {
        if (r.status == RobotStatus::MOVING && r.moveCooldown <= 0) {
            anyRobotMoved = true;
        }
        r.update();
    }

    if (anyRobotMoved) {
        systemTick++;
    }
}