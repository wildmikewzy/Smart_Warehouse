#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include "WarehouseManager.h"

WarehouseManager::WarehouseManager() {
    systemTick = 0;
    setupScene();
}

void WarehouseManager::setupScene() {
    // 1. 初始化空间检索缓存
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            gridToStationCache[x][y] = -1;
        }
    }
    int currentStationId = 1;

    // 2. 货架设障
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            if (x == 0 || x == 19 || y == 0 || y == 19) continue;
            if (x == 9 || x == 10 || y == 9 || y == 10) continue;

            bool inLeftCols = (x >= 2 && x <= 3) || (x >= 6 && x <= 7);
            bool inRightCols = (x >= 12 && x <= 13) || (x >= 16 && x <= 17);
            bool inTopRows = (y >= 2 && y <= 7);
            bool inBottomRows = (y >= 12 && y <= 17);

            if (!(inLeftCols || inRightCols) || !(inTopRows || inBottomRows)) continue;

            warehouseMap.setObstacle(x, y, 1);
            int dist = abs(x - 19) + abs(y - 10);
            SKUType skuType = (dist <= 12) ? SKUType::A : ((dist <= 20) ? SKUType::B : SKUType::C);

            Point dockPoint = (x % 2 == 0) ? Point{ x - 1, y } : Point{ x + 1, y };

            Station station;
            station.stationId = currentStationId++;
            station.shelfPoint = { x, y };
            station.dockPoint = dockPoint;
            station.sku = skuType;

            stations[station.stationId] = station;
            gridToStationCache[x][y] = station.stationId;
            if (dockPoint.x >= 0 && dockPoint.x < 20 && dockPoint.y >= 0 && dockPoint.y < 20) {
                gridToStationCache[dockPoint.x][dockPoint.y] = station.stationId;
            }
        }
    }

    // 3. 部署出货口站
    warehouseMap.setObstacle(19, 10, 2);
    Station outStation;
    outStation.stationId = 999;
    outStation.shelfPoint = { 19, 10 };
    outStation.dockPoint = { 18, 10 };
    outStation.sku = SKUType::A;
    stations[outStation.stationId] = outStation;
    gridToStationCache[19][10] = 999;
    gridToStationCache[18][10] = 999;

    // 4. 重置车辆并上"出生硬锁"
    robots.clear();
    Robot r1(1, { 0, 8 }); r1.status = RobotStatus::IDLE; robots.push_back(r1);
    Robot r2(2, { 0, 9 }); r2.status = RobotStatus::IDLE; robots.push_back(r2);
    Robot r3(3, { 0, 10 }); r3.status = RobotStatus::IDLE; robots.push_back(r3);
    Robot r4(4, { 0, 11 }); r4.status = RobotStatus::IDLE; robots.push_back(r4);

    // 【防穿模：闲置小车硬锁】
    for (auto& r : robots) {
        for (int t = 0; t < 2500; ++t) {
            globalTable.vertexReservations[{r.currentPos.x, r.currentPos.y, t}] = r.id;
        }
    }
}

void WarehouseManager::prepareStressTest(const std::vector<std::pair<int, Point>>& placements) {
    // 维持原貌
}

void WarehouseManager::dispatchRobot(int robotId, int stationId) {
    if (robotId < 1 || robotId > robots.size()) return;
    if (stations.find(stationId) == stations.end()) return;

    Station targetStation = stations[stationId];
    Point targetGrid = targetStation.dockPoint;

    // 清除该小车未来的所有旧时空预定（保留当前格子当前的预定）
    for (auto it = globalTable.vertexReservations.begin(); it != globalTable.vertexReservations.end(); ) {
        if (it->second == robotId && it->first.t >= systemTick) {
            it = globalTable.vertexReservations.erase(it);
        }
        else {
            ++it;
        }
    }

    // 时空寻路调度
    for (auto& r : robots) {
        if (r.id == robotId) {
            std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, warehouseMap, globalTable, robotId, systemTick, false);
            r.setPath(path);

            if (!path.empty()) {
                for (size_t i = 0; i < path.size(); ++i) {
                    int t = systemTick + static_cast<int>(i);
                    globalTable.vertexReservations[{path[i].x, path[i].y, t}] = robotId;

                    if (i > 0) {
                        std::string edgeKey = to_string(path[i - 1].x) + "," + to_string(path[i - 1].y) + "->" +
                            to_string(path[i].x) + "," + to_string(path[i].y) + "@" + to_string(t);
                        globalTable.edgeReservations.insert(edgeKey);
                    }
                }

                // 抵达目标站点后的时空追加霸占
                Point finalPos = path.back();
                int arrivalTime = systemTick + static_cast<int>(path.size()) - 1;
                for (int futureT = arrivalTime + 1; futureT < arrivalTime + 2500; ++futureT) {
                    globalTable.vertexReservations[{finalPos.x, finalPos.y, futureT}] = robotId;
                }
            }
            break;
        }
    }
}

void WarehouseManager::updateAll() {
    // 1. 每 15 个逻辑步（SystemTick）生成一个新的随机订单
    if (systemTick % 15 == 0) {
        std::vector<int> validShelfIds;
        for (const auto& pair : stations) {
            if (pair.first != 999) validShelfIds.push_back(pair.first);
        }
        if (!validShelfIds.empty()) {
            orderSystem.generateRandomOrder(validShelfIds);
        }
    }

    // 2. 核心调度轮询：为闲置的 AGV 分配任务
    Order* currentOrder = orderSystem.getFirstWaitingOrder();
    if (currentOrder != nullptr) {
        Robot* availableRobot = nullptr;
        for (auto& r : robots) {
            if (r.status == RobotStatus::IDLE) {
                availableRobot = &r;
                break;
            }
        }

        if (availableRobot != nullptr) {
            currentOrder->status = OrderStatus::PROCESSING;
            availableRobot->status = RobotStatus::MOVING_TO_PICK;
            availableRobot->currentTargetStationId = currentOrder->targetStationId;
            availableRobot->currentOrderId = currentOrder->orderId;
            dispatchRobot(availableRobot->id, currentOrder->targetStationId);
        }
    }

    // 3. 高频驱动小车肉体动画插值，并捕获步进脉冲信号
    bool anyRobotLogicStepped = false;
    for (auto& r : robots) {
        r.update(); // 步进小车的插值和冷却
        if (r.cellStepCompleted) {
            anyRobotLogicStepped = true; // 捕获到有小车在这一帧真正走完了一格
        }
    }

    // 4. 业务层状态机闭环控制（配合业务时钟）
    for (auto& r : robots) {
        // MOVING_TO_PICK 状态：必须在它完全挪进那一格（cellStepCompleted）且路径清空时才判定到达
        if (r.status == RobotStatus::MOVING_TO_PICK && r.pathQueue.empty() && (anyRobotLogicStepped || r.moveProgress == 0.0f)) {
            r.status = RobotStatus::LOADING;
            r.workCooldown = 143; // 保持你原先调试好的装货帧数
        }
        else if (r.status == RobotStatus::LOADING) {
            if (r.workCooldown > 0) {
                r.workCooldown--;
            }
            else {
                r.status = RobotStatus::MOVING_TO_DELIVER;
                dispatchRobot(r.id, 999); // 指派前往出货口
            }
        }
        // MOVING_TO_DELIVER 状态：同理，平滑完全进格后判定到达
        if (r.status == RobotStatus::MOVING_TO_DELIVER && r.pathQueue.empty() && (anyRobotLogicStepped || r.moveProgress == 0.0f)) {
            r.status = RobotStatus::UNLOADING;
            r.workCooldown = 86; // 保持你原先调试好的卸货帧数
        }
        else if (r.status == RobotStatus::UNLOADING) {
            if (r.workCooldown > 0) {
                r.workCooldown--;
            }
            else {
                orderSystem.completeOrder(r.currentOrderId);
                r.status = RobotStatus::IDLE;

                // 卸货完成，清除旧占位，回转返航
                for (auto it = globalTable.vertexReservations.begin(); it != globalTable.vertexReservations.end(); ) {
                    if (it->second == r.id) it = globalTable.vertexReservations.erase(it);
                    else ++it;
                }

                std::vector<Point> retPath = Router::getPath(r.currentPos, r.homePos, warehouseMap, globalTable, r.id, systemTick, false);
                r.setPath(retPath);

                if (!retPath.empty()) {
                    for (size_t i = 0; i < retPath.size(); ++i) {
                        globalTable.vertexReservations[{retPath[i].x, retPath[i].y, systemTick + (int)i}] = r.id;
                    }
                    int arrivalT = systemTick + static_cast<int>(retPath.size()) - 1;
                    for (int futT = arrivalT + 1; futT < arrivalT + 2500; ++futT) {
                        globalTable.vertexReservations[{retPath.back().x, retPath.back().y, futT}] = r.id;
                    }
                }
            }
        }
    }

    // 5. 【最核心修复】：只有当小车的肉体切实走完一网格，底层时间轴才推演一步
    if (anyRobotLogicStepped) {
        systemTick++;
    }
}