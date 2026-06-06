#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <windows.h>
#include "WarehouseManager.h"
#include "common.h"
#include "HungarianOptimizer.h"
#include <map>
#include <cmath>
/**
 * @brief WarehouseManager 的构造函数
 * @param 无
 * @retval 无
 * @details 负责整个智能仓储系统的全局系统推演时钟（systemTick）清零，并调用 setupScene 初始化整个静态仓库环境。
 */
WarehouseManager::WarehouseManager() {
    systemTick = 0;
    setupScene();
}

/**
 * @brief 静态仓库场景初始化函数
 * @param 无
 * @retval 无
 * @details 负责 20x20 地图的货架障碍物设置、取货点（Docking Point）拓扑绑定、出货口部署、
 * AGV小车出生部署，以及初始化状态下的闲置时空硬锁，全面构筑时空寻路的底层静态网格。
 */
void WarehouseManager::setupScene() {
    // ----------------------------------------------------------------
    // 1. 初始化空间检索缓存
    // ----------------------------------------------------------------
    // 将 20x20 网格的所有坐标缓存默认初始化为 -1，用于后续高频查询某个坐标是否对应站点
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            gridToStationCache[x][y] = -1;
        }
    }
    int currentStationId = 1; // 货架物理站点 ID 从 1 开始累加

    // ----------------------------------------------------------------
    // 2. 货架设障与拓扑站点绑定
    // ----------------------------------------------------------------
    // 采用结构化几何分布批量在地图中央生成货架矩阵，并将其设为不可通行的阻挡点
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            // 边缘留出外围通道环线，不设货架
            if (x == 0 || x == 19 || y == 0 || y == 19) continue;
            // 十字中央主干道十字十字交叉区留空，保证交通通畅
            if (x == 9 || x == 10 || y == 9 || y == 10) continue;

            // 根据列与行的几何规律，划定 4 个主要的货架聚集区块
            bool inLeftCols = (x >= 2 && x <= 3) || (x >= 6 && x <= 7);
            bool inRightCols = (x >= 12 && x <= 13) || (x >= 16 && x <= 17);
            bool inTopRows = (y >= 2 && y <= 7);
            bool inBottomRows = (y >= 12 && y <= 17);

            // 若不属于规划内的货架区块，则保持空地通道
            if (!(inLeftCols || inRightCols) || !(inTopRows || inBottomRows)) continue;

            // 在地图矩阵中将该坐标标记为不可通行的货架障碍物 (类型1)
            warehouseMap.setObstacle(x, y, 1);

            // 根据货架距离出货口(19, 10)的曼哈顿距离计算热度，分配货品 SKU 类别（A/B/C类）
            int dist = abs(x - 19) + abs(y - 10);
            SKUType skuType = (dist <= 12) ? SKUType::A : ((dist <= 20) ? SKUType::B : SKUType::C);

            // 关键拓扑绑定：偶数列小车从左侧靠泊，奇数列小车从右侧靠泊，生成精准的取货交互格点
            Point dockPoint = (x % 2 == 0) ? Point{ x - 1, y } : Point{ x + 1, y };

            // 构造 Station 对象并写入系统拓扑属性看板
            Station station;
            station.stationId = currentStationId++;
            station.shelfPoint = { x, y };
            station.dockPoint = dockPoint;
            station.sku = skuType;

            stations[station.stationId] = station;

            // 将货架实体格和对应的交互靠泊格共同注册进空间双重检索缓存表
            gridToStationCache[x][y] = station.stationId;
            if (dockPoint.x >= 0 && dockPoint.x < 20 && dockPoint.y >= 0 && dockPoint.y < 20) {
                gridToStationCache[dockPoint.x][dockPoint.y] = station.stationId;
            }
        }
    }
    // ----------------------------------------------------------------
    // 3. 部署统一的出货口站 (OUT站)
    // ----------------------------------------------------------------
    // 在右侧边界正中 (19, 10) 部署出货口，并标记为特殊障碍物类型 (类型2)
    warehouseMap.setObstacle(19, 10, 2);
    Station outStation;
    outStation.stationId = 999;     // 999 规定为全局唯一的出货口特殊 ID
    outStation.shelfPoint = { 19, 10 };
    outStation.dockPoint = { 18, 10 }; // 靠泊卸货格设在出货口左侧邻格
    outStation.sku = SKUType::A;
    stations[outStation.stationId] = outStation;

    // 注册出货口的空间查询索引
    gridToStationCache[19][10] = 999;
    gridToStationCache[18][10] = 999;

    // ----------------------------------------------------------------
    // 4. 重置车辆并施加“出生硬锁”
    // ----------------------------------------------------------------
    // 清空现有小车集合，并在左侧通道待命区依次生成 4 辆处于 IDLE 闲置状态的 AGV
    robots.clear();
    Robot r1(1, { 0, 8 }); r1.status = RobotStatus::IDLE; robots.push_back(r1);
    Robot r2(2, { 0, 9 }); r2.status = RobotStatus::IDLE; robots.push_back(r2);
    Robot r3(3, { 0, 10 }); r3.status = RobotStatus::IDLE; robots.push_back(r3);
    Robot r4(4, { 0, 11 }); r4.status = RobotStatus::IDLE; robots.push_back(r4);
    // =================================================================
// 🌟【新增：静态全车雷达清单完美绑定】
// =================================================================
// 必须定义一个跟 Robot::allRobots 类型 (std::vector<Robot*>*) 完全匹配的常驻容器。
// 使用 static 确保它的生命周期与整个程序执行周期一致，绝不随函数结束而析构。
    static std::vector<Robot*> robotPointerList;
    robotPointerList.clear();

    // 将 robots 容器里【已经完成 push_back、内存地址固定了的】真实小车对象的地址抓取出来
    for (auto& r : robots) {
        robotPointerList.push_back(&r);
    }

    // 功德圆满！将这个指针容器的地址正式灌给静态成员
    Robot::setRobotList(&robotPointerList);
    // 【防穿模机制：闲置占位霸占】
    // 小车在待命区未出勤时，必须在前瞻 2500 个时空Tick内强制写入时空预订表，防止其它出勤车穿模相撞
    for (auto& r : robots) {
        for (int t = 0; t < 2500; ++t) {
            globalTable.vertexReservations[{r.currentPos.x, r.currentPos.y, t}] = r.id;
        }
    }
}

/**
 * @brief 压力测试场景预备接口
 * @param placements 自定义配置的小车 ID 与点位对坐标映射集合
 * @retval 无
 */
void WarehouseManager::prepareStressTest(const std::vector<std::pair<int, Point>>& placements) {
    // 维持原貌，预留用于极端大并发调度时的特殊位置锁
}

/**
 * @brief 指派 AGV 小车前往指定目标站点的时空寻路核心调度函数
 * @param robotId 目标驱动的小车 ID
 * @param stationId 目标前往执行业务的站点拓扑 ID (货架站 ID 或出货口 999)
 * @retval 无
 * @details 负责清除该车辆未来的所有过期旧时空预订，调用时空 A* Router 算法计算绝对防撞的路径，
 * 并在路径生成后将整条路轨的时空点对（Vertex/Edge）原子性地写入全局 Reservation 表中。
 */
void WarehouseManager::dispatchRobot(int robotId, int stationId) {
    // 参数合法性边界拦截防护
    if (robotId < 1 || robotId > robots.size()) return;
    if (stations.find(stationId) == stations.end()) return;

    Station targetStation = stations[stationId];
    Point targetGrid = targetStation.dockPoint; // 抽取出具体的靠泊交互网格坐标

    // ----------------------------------------------------------------
    // 功能模块 A: 清除该小车未来的所有旧时空预定
    // ----------------------------------------------------------------
    // 必须抹去当前系统时间步 systemTick 以后的所有旧时空快照，否则车辆无法完成二次规划
    for (auto it = globalTable.vertexReservations.begin(); it != globalTable.vertexReservations.end(); ) {
        if (it->second == robotId && it->first.t >= systemTick) {
            it = globalTable.vertexReservations.erase(it);
        }
        else {
            ++it;
        }
    }


    // ----------------------------------------------------------------
    // 功能模块 B: 时空寻路规划与时空轨迹预订上锁
    // ----------------------------------------------------------------
    // 匹配目标机器人，拉取时空 A* 路由器计算安全无冲突的最优路径轨道
    for (auto& r : robots) {
        if (r.id == robotId) {
            std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, warehouseMap, globalTable, robotId, systemTick, false);
            r.setPath(path); // 注入小车的移动路径队列中
            // 【新增安全阀】：如果拥堵到连时空 A* 都找不到路了，让它在原地等 1 步
            if (path.empty()) {
                std::cout << "[警报] 小车 " << robotId << " 寻路失败，返回空路径！当前系统 Tick: " << systemTick << std::endl;
                std::vector<Point> waitPath = { r.currentPos, r.currentPos }; // 原地等待路径
                r.setPath(waitPath);
                globalTable.vertexReservations[{r.currentPos.x, r.currentPos.y, systemTick + 1}] = robotId;
            }
            // 若寻路成功且路径不为空，开始进行刚性时空排他性占位
            if (!path.empty()) {
                for (size_t i = 0; i < path.size(); ++i) {
                    int t = systemTick + static_cast<int>(i); // 换算出未来精准的时间刻度

                    // 节点预订（Vertex Reservation）：在此时间点，该格子专属该车辆，防追尾/对头碰
                    globalTable.vertexReservations[{path[i].x, path[i].y, t}] = robotId;

                    // 边预订（Edge Reservation）：防对穿。不允许两辆车在相邻两帧互相交换格子位置
                    if (i > 0) {
                        std::string edgeKey = to_string(path[i - 1].x) + "," + to_string(path[i - 1].y) + "->" +
                            to_string(path[i].x) + "," + to_string(path[i].y) + "@" + to_string(t);
                        globalTable.edgeReservations.insert(edgeKey);
                    }
                }

                // ----------------------------------------------------------------
                // 功能模块 C: 抵达目标站点后的时空追加霸占
                // ----------------------------------------------------------------
                // 当小车走完路径停留在终点等待装卸货时，依然需要对其所占网格实施未来大范围的时空硬锁，
                // 彻底断绝其它车辆规划时直接横穿该停留小车肉体的可能性
                Point finalPos = path.back();
                int arrivalTime = systemTick + static_cast<int>(path.size()) - 1;
                if (stationId == 999) {
                    // 如果前往出货口，仅精准死锁这辆小车预计所需的卸货冷却时间(86步)，留出时空给后车排队！
                    for (int futureT = arrivalTime + 1; futureT < arrivalTime + 60; ++futureT) {
                        globalTable.vertexReservations[{finalPos.x, finalPos.y, futureT}] = robotId;
                    }
                }
                else {
                    // 如果是去普通货架，继续保留无限期硬占，防止被别的车冲刷
                    for (int futureT = arrivalTime + 1; futureT < arrivalTime + 2500; ++futureT) {
                        globalTable.vertexReservations[{finalPos.x, finalPos.y, futureT}] = robotId;
                    }
                }
            }
            break;
        }
    }
}
/**
* @brief 辅助函数：寻找当前排队直道上最靠前（最右侧）的空闲网格坐标 X
* @note 站在入口 (14,10) 的视角，动态扫描当前直道上真正最靠前的空闲 X
*/

int getNextAvailableQueueX_AtGate(const std::vector<Robot>& robots) {
    // 从队头 18 倒着扫到大门 15
    for (int x = 18; x >= 15; --x) {
        bool isOccupied = false;
        for (const auto& r : robots) {

            // 如果某辆车真的横在这一格，并且它【仍然处于送货或排队状态】
            // 注意：如果它的状态是 UNLOADING 或者是 RETURNING_BUFFER，它已经不属于“排队等待分配”的车了！
            // 但是在 (18,10) 正在卸货的车，依然要占住 18 号位，不让后车追尾。
            if (r.status == RobotStatus::UNLOADING && x == 18) {
                isOccupied = true;
                break;
            }

            // 如果其他车还在排队道上霸占着位置
            if (r.status == RobotStatus::MOVING_TO_DELIVER) {
                if (r.currentPos.y == 10 && r.currentPos.x == x) {
                    isOccupied = true;
                    break;
                }
                if (!r.pathQueue.empty() && r.pathQueue.back().y == 10 && r.pathQueue.back().x == x) {
                    isOccupied = true;
                    break;
                }
            }
        }

        if (!isOccupied) {
            return x; // 这一格是安全的黄金空位
        }
    }
    return 15;
}
/**
 * @brief 系统主循环核心每帧逻辑驱动、状态机闭环及多车时间步强对齐核心函数
 * @details 完美修复“先回巢再接单”硬伤，实现就近无缝运筹派单
 */
void WarehouseManager::updateAll() {
    // ====================================================================
	// 测试阶段：初始生成4个随机生成 4 个订单 ，并且按 T 键可以刷新随机订单
    // ====================================================================
    static bool ordersGenerated = false;
    if (!ordersGenerated) {
        std::vector<int> validShelfIds;
        for (const auto& pair : stations) {
            if (pair.first != 999) validShelfIds.push_back(pair.first);
        }

        if (!validShelfIds.empty()) {
            for (int i = 0; i < 4; ++i) {
                orderSystem.generateRandomOrder(validShelfIds);
            }
            ordersGenerated = true;
        }
    }
    static bool lastTState = false;
    bool currentTState = (GetAsyncKeyState('T') & 0x8000) != 0;

    if (currentTState && !lastTState) {
        ordersGenerated = false;
    }
    lastTState = currentTState;
    // ===========================================================
    // 🌟【核心新增】：P 键触发 20 个绝对相同的并发压力测试订单流
    // ===========================================================
    static bool lastPState = false; // 记录上一帧 P 键是否被按下
    bool currentPState = (GetAsyncKeyState('P') & 0x8000) != 0; // 当前帧 P 键状态

    // 检测到 P 键单次敲击瞬间
    if (currentPState && !lastPState) {
        // 1. 强力清空当前订单系统，保证测试跑道绝对纯净
        orderSystem.clearAllOrders();

        // 2. 定义 20 个确定性的压力测试订单流 (Pair: <货架ID, SKU类型int>)
        // 货架ID严格参照你的地图图片，覆盖灰、蓝、橙三大温区
        std::vector<std::pair<int, int>> stressTestData = {
            {3, 0},  {14, 1}, {17, 0}, {26, 2}, {41, 0},  // 前5单：偏左侧与中部
            {53, 1}, {64, 0}, {75, 1}, {87, 2}, {9, 0},   // 中5单：向右侧橙色重度区延伸
            {22, 0}, {32, 1}, {45, 0}, {48, 2}, {57, 1},  // 后5单：上下两侧深度穿插
            {68, 0}, {80, 0}, {93, 1}, {6, 2},  {91, 0}   // 尾5单：极限边缘对角线单
        };

        // 3. 刚性批量灌入订单系统大盘
        for (const auto& testOrder : stressTestData) {
            orderSystem.injectCustomOrder(testOrder.first, testOrder.second);
        }

         //4. 重置系统总时钟（可选）：让两边的性能对比都从同一个时间起点起跑
         systemTick = 0; 
    }
    lastPState = currentPState; // 更新按键历史状态
    // ==================================================
    // 【时空 A* 寻路限流阀】
    // ==================================================
    bool pathCalculatedThisFrame = false;

    // 每帧滚动等待中订单的痛苦指数
    orderSystem.updateOrderTicks();

    // ==================================================
    // 🌟【运筹调度层】：基于匈牙利算法的全局集中匹配
    // ==================================================
    if (!pathCalculatedThisFrame) {

        // 1. 抓取真正能接单的车（状态为 IDLE 且没有紧急任务路径）
        std::vector<Robot*> freeRobots;
        for (auto& r : robots) {
            if (r.status == RobotStatus::IDLE && r.pathQueue.empty()) {
                freeRobots.push_back(&r);
            }
        }

        std::vector<Order*> waitingOrders;
        for (auto& o : orderSystem.getNonConstActiveOrders()) {
            if (o.status == OrderStatus::WAITING) {
                waitingOrders.push_back(&o);
            }
        }

        // 2. 只有当“有车闲着”且“有单等着”时，才启动数学矩阵计算
        if (!freeRobots.empty() && !waitingOrders.empty()) {
            int nRows = (int) freeRobots.size();
            int nCols = (int)waitingOrders.size();
            std::vector<std::vector<int>> costMatrix(nRows, std::vector<int>(nCols, 0));

            const int TIME_WEIGHT = 2;

            for (int i = 0; i < nRows; ++i) {
                for (int j = 0; j < nCols; ++j) {
                    int stationId = waitingOrders[j]->targetStationId;
                    Station targetStation = this->getStationById(stationId);
                    Point dockPos = targetStation.dockPoint;

                    // 🌟【就近核心】：此时由于小车可能在 (14,9) 刚送完货，这里计算出来的曼哈顿距离就是小车在送货口附近的实时距离！
                    int curX = static_cast<int>(freeRobots[i]->realX + 0.5f);
                    int curY = static_cast<int>(freeRobots[i]->realY + 0.5f);
                    int manhattanDist = abs(curX - dockPos.x) + abs(curY - dockPos.y);

                    int cost = manhattanDist * 10 - (waitingOrders[j]->waitTicks / TIME_WEIGHT);
                    costMatrix[i][j] = (cost > 0) ? cost : 0;
                }
            }

            HungarianOptimizer optimizer;
            std::vector<int> assignment;
            optimizer.solve(costMatrix, assignment);

            for (int i = 0; i < nRows; ++i) {
                int assignedOrderIdx = assignment[i];
                if (assignedOrderIdx != -1 && assignedOrderIdx < nCols) {
                    Robot* robot = freeRobots[i];
                    Order* order = waitingOrders[assignedOrderIdx];

                    if (!pathCalculatedThisFrame) {
                        order->status = OrderStatus::PROCESSING;
                        robot->status = RobotStatus::MOVING_TO_PICK;
                        robot->currentTargetStationId = order->targetStationId;
                        robot->currentOrderId = order->orderId;

                        // 核心：直接从小车当前所处的任何位置（包括排队撤离点）发起就近时空寻路！
                        dispatchRobot(robot->id, order->targetStationId);

                        pathCalculatedThisFrame = true;
                        break;
                    }
                }
            }
        }
    }

    // ==================================================
    // 2. 高频驱动小车肉体动画插值 (保持原样)
    // ==================================================
    bool anyRobotLogicStepped = false;
    bool anyRobotWorking = false;

    for (auto& r : robots) {
        r.update();
        if (r.cellStepCompleted) anyRobotLogicStepped = true;
        if (r.status == RobotStatus::LOADING || r.status == RobotStatus::UNLOADING || r.status == RobotStatus::RETURNING_BUFFER) {
            anyRobotWorking = true;
        }
    }

    // ==================================================
    // 3. 双轨硬核传送带状态机
    // ==================================================
    for (auto& r : robots) {
        // ---------- 阶段一：去货架取货与装载 ----------
        if (r.status == RobotStatus::MOVING_TO_PICK && r.pathQueue.empty() && (r.cellStepCompleted || r.moveProgress == 0.0f)) {
            r.status = RobotStatus::LOADING;
            r.workCooldown = 30;
        }
        else if (r.status == RobotStatus::LOADING) {
            if (r.workCooldown > 0) {
                r.workCooldown--;
            }
            else {
                r.status = RobotStatus::MOVING_TO_DELIVER;
                std::vector<Point> toGatePath = Router::getPath(r.currentPos, Point(15, 10), warehouseMap, globalTable, r.id, systemTick, false);
                r.setPath(toGatePath);
            }
        }

        // ---------- 阶段二：送货路上与门禁靠泊递补排队 ----------
        else if (r.status == RobotStatus::MOVING_TO_DELIVER) {

            // 🌟【第一步：触头检测】：只要肉体踩到 (18, 10) 且路径空了，立刻强转卸货状态 (保持原样)
            if (r.currentPos == Point(18, 10) && r.pathQueue.empty()) {
                r.status = RobotStatus::UNLOADING;
                r.workCooldown = 40;
                continue;
            }

            // 🌟【第二步：刚性逐格看前车递补】：针对卡在直道排队区（15~17, 10）且没有路径的后车 (保持原样)
            if (r.currentPos.y == 10 && r.currentPos.x >= 15 && r.currentPos.x <= 17 && r.pathQueue.empty()) {
                int nextX = r.currentPos.x + 1;
                bool nextCellOccupied = false;

                for (const auto& other : robots) {
                    if (other.status == RobotStatus::UNLOADING && nextX == 18) {
                        nextCellOccupied = true;
                        break;
                    }
                    if (other.status == RobotStatus::MOVING_TO_DELIVER) {
                        if (other.currentPos.y == 10 && other.currentPos.x == nextX) {
                            nextCellOccupied = true;
                            break;
                        }
                        if (!other.pathQueue.empty() && other.pathQueue.back().y == 10 && other.pathQueue.back().x == nextX) {
                            nextCellOccupied = true;
                            break;
                        }
                    }
                }

                if (!nextCellOccupied) {
                    std::vector<Point> stepForwardPath;
                    stepForwardPath.push_back(Point(nextX, 10));
                    r.pathQueue.clear();
                    r.setPath(stepForwardPath);
                    r.moveProgress = 0.0f;
                    r.cellStepCompleted = false;
                }
            }
            // 🌟【新增第三步：中途拥堵丢路径兜底重寻路】：完美收容处于大地图中、因拥堵导致路径为空的小车
            else if (r.pathQueue.empty()) {
                // 严格遵守单帧时空 A* 寻路限流阀，绝对不挤爆 CPU 算力
                if (!pathCalculatedThisFrame) {
                    // 原地重新向大门 (15, 10) 发起时空避障寻路请求
                    std::vector<Point> toGatePath = Router::getPath(r.currentPos, Point(15, 10), warehouseMap, globalTable, r.id, systemTick, false);

                    if (!toGatePath.empty()) {
                        r.setPath(toGatePath); // 寻路成功，重新注入移动灵魂！
                    }

                    // 🚨 绝杀细节：无论这次重寻路是成功还是由于拥堵再次失败，
                    // 只要调用了高耗时的 Router::getPath()，本帧的寻路限流阀都必须强行锁死，保护帧率！
                    pathCalculatedThisFrame = true;
                }
            }
        }
        // ---------- 阶段三：队头正忙，开始卸货 ----------
        else if (r.status == RobotStatus::UNLOADING) {
            if (r.workCooldown > 0) {
                r.workCooldown--;
            }
            else {
                r.status = RobotStatus::RETURNING_BUFFER;
                orderSystem.completeOrder(r.currentOrderId);
                r.currentOrderId = -1;

                std::vector<Point> escapePath;
                escapePath.push_back(Point(18, 9));
                escapePath.push_back(Point(17, 9));
                escapePath.push_back(Point(16, 9));
                escapePath.push_back(Point(15, 9));
                escapePath.push_back(Point(14, 9));
                r.setPath(escapePath);
            }
        }
        // ---------- 阶段四：侧边道撤离完毕决策点 ----------
        else if (r.status == RobotStatus::RETURNING_BUFFER) {
            if (r.pathQueue.empty()) {
                // 🌟【超级进化】：到达安全区后，原地转为 IDLE 待命，但绝对不规划路径！
                // 让它赤条条地进入下一帧，直接接受匈牙利算法的全局扫描
                r.status = RobotStatus::IDLE;
                r.currentTargetStationId = -1;
            }
        }
        // ---------- 🌟【新增业务兜底】：真正无单可做时，车子才大后方回巢 ----------
        else if (r.status == RobotStatus::IDLE && r.pathQueue.empty()) {
            // 如果小车处于闲置状态，且经历过运筹层的扫描后依然没有被分发任务（说明订单池空了）
            // 并且小车此时并不在自己的出生大本营，这时候再触发“防挂机回巢”机制
            Point homePos = { 0, 7 + r.id };
            if (r.currentPos.x != homePos.x || r.currentPos.y != homePos.y) {
                Point homeGate = { 1, 7 + r.id };
                std::vector<Point> toHomePath = Router::getPath(r.currentPos, homeGate, warehouseMap, globalTable, r.id, systemTick, false);
                toHomePath.push_back(homePos);
                r.setPath(toHomePath);
            }
        }
    }

    // 4. 时钟步进脉冲控制
    if (anyRobotLogicStepped || anyRobotWorking) {
        systemTick++;
    }
}
/**
 * @brief 🌟【全局运筹优化核心】：基于匈牙利算法的上帝视角全局集中式调度
 */
void WarehouseManager::dispatchOrdersByHungarian() {
    // 1. 每帧先让订单的等待时间滚动起来
    orderSystem.updateOrderTicks();

    // 2. 搜集快照：挑出场上所有真正“闲得发慌”的小车和“嗷嗷待哺”的订单
    std::vector<Robot*> freeRobots;
    for (auto& r : robots) {
        if (r.status == RobotStatus::IDLE) { // 只有处于空闲、回巢待命状态的车才参与竞标
            freeRobots.push_back(&r);
        }
    }

    std::vector<Order*> waitingOrders;
    // 获取当前 activeOrders 的肉体指针（可以遍历 getAllOrders 的底层引用，或者给 OrderSystem 增加一个返回非const活跃列表的接口）
    // 这里假设通过类似 getWaitingOrdersList() 或者直接访问内部容器拿到所有等待单
    for (auto& o : orderSystem.getNonConstActiveOrders()) {
        if (o.status == OrderStatus::WAITING) {
            waitingOrders.push_back(&o);
        }
    }

    // 防御保护：如果场上没有空闲车，或者没有等待的订单，直接退出，不浪费 CPU 算力
    if (freeRobots.empty() || waitingOrders.empty()) {
        return;
    }

    // 3. 构建二维综合“代价矩阵” (行: 空闲小车, 列: 等待订单)
    int nRows = (int)freeRobots.size();
    int nCols = (int)waitingOrders.size();
    std::vector<std::vector<int>> costMatrix(nRows, std::vector<int>(nCols, 0));

    // 设定时间防饥饿项的权重因子
    const int TIME_WEIGHT = 2;

    for (int i = 0; i < nRows; ++i) {
        for (int j = 0; j < nCols; ++j) {
            // a. 拿到该订单对应的货架靠泊点（RCS 目的地）
            int stationId = waitingOrders[j]->targetStationId;
            Station targetStation = this->getStationById(stationId); // 假设你有通过 ID 获取 Station 的查表函数
            Point dockPos = targetStation.dockPoint;

            // b. 计算小车当前真实位置到货架靠泊点的曼哈顿距离
            int curX = static_cast<int>(freeRobots[i]->realX + 0.5f);
            int curY = static_cast<int>(freeRobots[i]->realY + 0.5f);
            int manhattanDist = abs(curX - dockPos.x) + abs(curY - dockPos.y);

            // c. 运用痛苦指数公式：综合代价 = 曼哈顿距离成本 - 饥饿惩罚项
            int cost = manhattanDist * 10 - (waitingOrders[j]->waitTicks / TIME_WEIGHT);

            // 匈牙利算法要求代价不能为负数，做个刚性底线防御
            costMatrix[i][j] = (cost > 0) ? cost : 0;
        }
    }

    // 4. 召唤匈牙利算法求解器，开启离散数学奇迹
    HungarianOptimizer optimizer;
    std::vector<int> assignment; // 存储第 i 辆车分到的订单索引
    optimizer.solve(costMatrix, assignment);

    // 5. 根据最优匹配结果，进行全局刚性批量下发
    for (int i = 0; i < nRows; ++i) {
        int assignedOrderIdx = assignment[i];

        // 如果这辆车成功匹配到了一个真实的订单（非虚拟补齐节点）
        if (assignedOrderIdx != -1 && assignedOrderIdx < nCols) {
            Robot* robot = freeRobots[i];
            Order* order = waitingOrders[assignedOrderIdx];

            // a. 改变订单状态为执行中
            order->status = OrderStatus::PROCESSING;

            // b. 刚性绑定小车的业务上下文，注入灵魂
            robot->status = RobotStatus::MOVING_TO_PICK; // 小车状态强切：全速前往货架取货
            robot->currentOrderId = order->orderId;
            robot->currentTargetStationId = order->targetStationId;

            // c. 驱动底层 RCS 动力马达开始寻路导航
            this->dispatchRobot(robot->id, order->targetStationId);
        }
    }
}