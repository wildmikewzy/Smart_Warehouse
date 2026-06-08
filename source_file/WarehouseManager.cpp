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
#include "Robot.h"
#include "GUI.h"

/**
 * @brief WarehouseManager 的构造函数
 * @param 无
 * @retval 无
 * @note 负责整个智能仓储系统的全局系统推演时钟（systemTick）清零，并调用 setupScene 初始化整个静态仓库环境。
 */
WarehouseManager::WarehouseManager() {
    systemTick = 0;
    setupScene();
}

/**
 * @brief 静态仓库场景初始化函数
 * @param 无
 * @retval 无
 * @note 负责 20x20 地图的货架障碍物设置、取货点（Docking Point）拓扑绑定、出货口部署、
 * AGV小车出生部署，以及初始化状态下的闲置时空硬锁，全面构筑时空寻路的底层静态网格。
 */
void WarehouseManager::setupScene() {
    // ----------------------------------------------------------------
    // 1. 初始化空间检索缓存
    // ----------------------------------------------------------------
    // 步骤 1.1：将 20x20 网格的所有空间坐标缓存默认初始化为 -1，用于后续高频查询某个坐标是否对应站点
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            gridToStationCache[x][y] = -1;
        }
    }
    int currentStationId = 1; // 货架物理站点 ID 从 1 开始累加

    // ----------------------------------------------------------------
    // 2. 货架设障与拓扑站点绑定
    // ----------------------------------------------------------------
    // 步骤 2.1：采用结构化几何分布批量在地图中央生成货架矩阵，并将其设为不可通行的阻挡点
    for (int x = 0; x < 20; ++x) {
        for (int y = 0; y < 20; ++y) {
            // 步骤 2.1.1：边缘留出外围通道环线，不设货架障碍
            if (x == 0 || x == 19 || y == 0 || y == 19) continue;
            // 步骤 2.1.2：十字中央主干道交叉区留空，保证核心交通枢纽通畅
            if (x == 9 || x == 10 || y == 9 || y == 10) continue;

            // 步骤 2.2：根据列与行的几何规律，划定 4 个主要的货架聚集区块
            bool inLeftCols = (x >= 2 && x <= 3) || (x >= 6 && x <= 7);
            bool inRightCols = (x >= 12 && x <= 13) || (x >= 16 && x <= 17);
            bool inTopRows = (y >= 2 && y <= 7);
            bool inBottomRows = (y >= 12 && y <= 17);

            // 若不属于规划内的货架区块，则保持空地通道状态
            if (!(inLeftCols || inRightCols) || !(inTopRows || inBottomRows)) continue;

            // 步骤 2.3：在地图矩阵中将该坐标标记为不可通行的货架障碍物 (类型1)
            warehouseMap.setObstacle(x, y, 1);

            // 步骤 2.4：根据货架距离出货口(19, 10)的曼哈顿距离计算热度，分配货品 SKU 类别（A/B/C类温区）
            int dist = abs(x - 19) + abs(y - 10);
            SKUType skuType = (dist <= 12) ? SKUType::A : ((dist <= 20) ? SKUType::B : SKUType::C);

            // 步骤 2.5：关键拓扑绑定：偶数列小车从左侧靠泊，奇数列小车从右侧靠泊，生成精准的取货交互格点
            Point dockPoint = (x % 2 == 0) ? Point{ x - 1, y } : Point{ x + 1, y };

            // 步骤 2.6：构造 Station 对象并写入系统拓扑属性看板
            Station station;
            station.stationId = currentStationId++;
            station.shelfPoint = { x, y };
            station.dockPoint = dockPoint;
            station.sku = skuType;

            stations[station.stationId] = station;

            // 步骤 2.7：将货架实体格和对应的交互靠泊格共同注册进空间双重检索缓存表中
            gridToStationCache[x][y] = station.stationId;
            if (dockPoint.x >= 0 && dockPoint.x < 20 && dockPoint.y >= 0 && dockPoint.y < 20) {
                gridToStationCache[dockPoint.x][dockPoint.y] = station.stationId;
            }
        }
    }

    // ----------------------------------------------------------------
    // 3. 部署统一的出货口站 (OUT站)
    // ----------------------------------------------------------------
    // 步骤 3.1：在右侧边界正中 (19, 10) 部署出货口，并标记为特殊障碍物类型 (类型2)
    warehouseMap.setObstacle(19, 10, 2);
    Station outStation;
    outStation.stationId = 999;     // 999 规定为全局唯一的出货口特殊 ID
    outStation.shelfPoint = { 19, 10 };
    outStation.dockPoint = { 18, 10 }; // 靠泊卸货格设在出货口左侧邻格
    outStation.sku = SKUType::A;
    stations[outStation.stationId] = outStation;

    // 步骤 3.2：注册出货口的空间高频查询索引
    gridToStationCache[19][10] = 999;
    gridToStationCache[18][10] = 999;

    // ----------------------------------------------------------------
    // 4. 重置车辆并施加“初始位姿刚性时空锁定”
    // ----------------------------------------------------------------
    // 步骤 4.1：清空现有小车集合，并在左侧通道待命区依次生成 4 辆处于 IDLE 闲置状态的 AGV
    robots.clear();
    Robot r1(1, { 0, 8 }); r1.status = RobotStatus::IDLE; robots.push_back(r1);
    Robot r2(2, { 0, 9 }); r2.status = RobotStatus::IDLE; robots.push_back(r2);
    Robot r3(3, { 0, 10 }); r3.status = RobotStatus::IDLE; robots.push_back(r3);
    Robot r4(4, { 0, 11 }); r4.status = RobotStatus::IDLE; robots.push_back(r4);

    // 步骤 4.2：动态构建与底层静态雷达清单完美绑定的常驻指针容器
    // 必须定义一个跟 Robot::allRobots 类型 (std::vector<Robot*>*) 完全匹配的常驻容器。
    // 使用 static 确保它的生命周期与整个程序执行周期一致，绝不随函数结束而析构。
    static std::vector<Robot*> robotPointerList;
    robotPointerList.clear();

    // 步骤 4.3：将 robots 容器里【已经完成 push_back、内存地址固定了的】真实小车对象的地址提取出来
    for (auto& r : robots) {
        robotPointerList.push_back(&r);
    }

    // 步骤 4.4：将该指针容器的基地址正式绑定至 Robot 类的静态数轴成员中
    Robot::setRobotList(&robotPointerList);

    // 步骤 4.5：【防穿模碰撞机制：闲置时空占位锁闭】
    // 智能体在待命区未出勤时，必须在前瞻 2500 个时空 Tick 内强制写入时空预订表，防止其它作业状态的智能体发生空间资源竞争而导致穿模相撞
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
 * @note 维持原貌，预留用于极端大并发调度时的特殊位置锁。
 */
void WarehouseManager::prepareStressTest(const std::vector<std::pair<int, Point>>& placements) {
    // 维持原貌，预留用于极端大并发调度时的特殊位置锁
}

/**
 * @brief 指派 AGV 小车前往指定目标站点的时空寻路核心调度函数
 * @param robotId 目标驱动的小车 ID
 * @param stationId 目标前往执行业务的站点拓扑 ID (货架站 ID 或出货口 999)
 * @retval 无
 * @note 负责原子性地清除该车辆未来的所有过期旧时空预订，调用时空 A* Router 算法计算绝对零冲突的路径，
 * 并在路径生成后将整条路轨的时空点对（Vertex/Edge）刚性写入全局 Reservation 表中。
 */
void WarehouseManager::dispatchRobot(int robotId, int stationId) {
    // 步骤 1.1：参数合法性边界拦截防护，防止非法索引引发动态容器崩溃
    if (robotId < 1 || robotId > robots.size()) return;
    if (stations.find(stationId) == stations.end()) return;

    Station targetStation = stations[stationId];
    Point targetGrid = targetStation.dockPoint; // 抽取出具体的靠泊交互网格坐标

    // ----------------------------------------------------------------
    // 功能模块 A: 清除该小车未来的所有旧时空预定
    // ----------------------------------------------------------------
    // 步骤 2.1：必须抹去当前系统时间步 systemTick 以后的所有旧时空快照，否则车辆无法完成二次重规划
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
    // 步骤 3.1：匹配目标机器人，拉取时空 A* 路由器计算安全无冲突的最优路径轨迹
    for (auto& r : robots) {
        if (r.id == robotId) {
            std::vector<Point> path = Router::getPath(r.currentPos, targetGrid, warehouseMap, globalTable, robotId, systemTick, false);
            r.setPath(path); // 注入小车的移动路径队列中

            // 步骤 3.2：【防死锁安全阀】：如果通道极度拥堵导致时空 A* 算法无法即刻求解，让其在原地等待 1 个步长以保护系统级推演
            if (path.empty()) {
                std::cout << "[警报] 小车 " << robotId << " 寻路失败，返回空路径！当前系统 Tick: " << systemTick << std::endl;
                std::vector<Point> waitPath = { r.currentPos, r.currentPos }; // 构造原地等待路径轨迹
                r.setPath(waitPath);
                globalTable.vertexReservations[{r.currentPos.x, r.currentPos.y, systemTick + 1}] = robotId;
            }

            // 步骤 3.3：若寻路成功且路径有效，开始进行刚性时空排他性锁闭
            if (!path.empty()) {
                for (size_t i = 0; i < path.size(); ++i) {
                    int t = systemTick + static_cast<int>(i); // 换算出未来精准的时间离散刻度

                    // 步骤 3.3.1：节点预订（Vertex Reservation）：在此时间点，该格子专属该车辆，全面杜绝追尾冲突
                    globalTable.vertexReservations[{path[i].x, path[i].y, t}] = robotId;

                    // 步骤 3.3.2：边预订（Edge Reservation）：防对穿。不允许两辆车在相邻两帧互相交换格子位置
                    if (i > 0) {
                        std::string edgeKey = to_string(path[i - 1].x) + "," + to_string(path[i - 1].y) + "->" +
                            to_string(path[i].x) + "," + to_string(path[i].y) + "@" + to_string(t);
                        globalTable.edgeReservations.insert(edgeKey);
                    }
                }

                // ----------------------------------------------------------------
                // 功能模块 C: 抵达目标站点后的时空追加锁闭
                // ----------------------------------------------------------------
                // 步骤 4.1：当小车走完路径停留在终点等待装卸货时，依然需要对其所占网格实施未来大范围的时空硬锁，
                // 彻底断绝其它车辆规划时直接横穿该停留小车物理实体的可能性
                Point finalPos = path.back();
                int arrivalTime = systemTick + static_cast<int>(path.size()) - 1;

                if (stationId == 999) {
                    // 步骤 4.1.1：如果前往出货口，仅精准锁定这辆小车预计所需的卸货作业时间(60步)，留出时空给后车执行排队递补
                    for (int futureT = arrivalTime + 1; futureT < arrivalTime + 60; ++futureT) {
                        globalTable.vertexReservations[{finalPos.x, finalPos.y, futureT}] = robotId;
                    }
                }
                else {
                    // 步骤 4.1.2：如果是去普通货架，继续保留超长时间步的硬性占位，防止被出勤小车时空穿透
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
 * @param robots 全局移动机器人集群容器的常量引用
 * @retval int 返回真正空闲且安全的黄金排队位离散横轴坐标 X
 * @note 站在入口 (14,10) 的全景视角，动态扫描当前直道上真正最靠前的空闲网格空间。
 */
int getNextAvailableQueueX_AtGate(const std::vector<Robot>& robots) {
    // 步骤 1.1：从排队道头部 18 逆向扫描至大门入口处 15
    for (int x = 18; x >= 15; --x) {
        bool isOccupied = false;
        for (const auto& r : robots) {

            // 步骤 1.2：如果某辆车横在这一格，并且它【仍然处于送货或排队状态】
            // 注意：如果它的状态是 UNLOADING 或者是 RETURNING_BUFFER，说明它已经脱离了“排队等待分配”序列
            // 但在 (18,10) 正在执行卸货作业的车，依然要占住 18 号位，防止后车追尾。
            if (r.status == RobotStatus::UNLOADING && x == 18) {
                isOccupied = true;
                break;
            }

            // 步骤 1.3：检测其他车辆是否还在排队直道上占用着对应位置
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

        // 步骤 1.4：若经全盘扫描未发现任何智能体占用，判定该格为安全的空闲位
        if (!isOccupied) {
            return x;
        }
    }
    return 15;
}

/**
 * @brief 系统主循环核心每帧逻辑驱动、状态机闭环及多车时间步强对齐核心函数
 * @param gui 负责平滑渲染与可视化交互的 GUI 实例引用
 * @retval 无
 * @note 完美打通了“原位动态接单”与“就近无缝运筹”的数据链路，驱动整体仓储调度流水线的自闭环运行。
 */
void WarehouseManager::updateAll(GUI& gui) {
    // ====================================================================
    // 1. 交互式随机订单流触发事件监听
    // ====================================================================
    static bool ordersGenerated = true;
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

    // 步骤 1.1：捕捉键盘 T 键的单次敲击瞬间（上升沿拦截），释放订单生成开关
    if (currentTState && !lastTState) {
        ordersGenerated = false;
    }
    lastTState = currentTState;

    // ===========================================================
    // 2. 高密并发压力测试数据流注入
    // ===========================================================
    static bool lastPState = false; // 记录上一帧 P 键是否被按下
    bool currentPState = (GetAsyncKeyState('P') & 0x8000) != 0; // 当前帧 P 键状态

    // 步骤 2.1：检测到 P 键敲击上升沿瞬间，刚性切入极端高压冲突测试环境
    if (currentPState && !lastPState) {
        // 步骤 2.1.1：强力清空当前订单系统大盘，保证测试跑道绝对纯净
        orderSystem.clearAllOrders();

        // 步骤 2.1.2：定义 20 个确定性的压力测试基准订单流 (数据对: <货架拓扑ID, SKU类型编码>)
        // 货架ID严格参照静态拓扑分布，全面覆盖灰、蓝、橙三大温区
        std::vector<std::pair<int, int>> stressTestData = {
            {3, 0},  {14, 1}, {17, 0}, {26, 2}, {41, 0},  // 前5单：偏左侧与中部密集区
            {53, 1}, {64, 0}, {75, 1}, {87, 2}, {9, 0},   // 中5单：向右侧橙色重度冲突区延伸
            {22, 0}, {32, 1}, {45, 0}, {48, 2}, {57, 1},  // 后5单：上下两侧高维度空间穿插
            {68, 0}, {80, 0}, {93, 1}, {6, 2},  {91, 0}   // 尾5单：极限边缘对角线超长距调度
        };

        // 步骤 2.1.3：刚性批量灌入订单系统核心缓冲池
        for (const auto& testOrder : stressTestData) {
            orderSystem.injectCustomOrder(testOrder.first, testOrder.second);
        }

        // 步骤 2.1.4：核心推演时钟与效能统计指标硬回零，确保两组运筹算法运行在绝对对齐的物理起跑线上
        systemTick = 0;
        globalSystemTick = 0;
        totalCompletedOrders = 0;
        totalOrderDuration = 0;

        // 步骤 2.1.5：时钟归零后，批量刷新这 20 个压测订单的诞生时间戳，使 createTick 与压测跑道完全同步
        for (auto& order : orderSystem.getAllOrders()) {
            this->onOrderCreated(order);
        }

        // 步骤 2.1.6：标记压测流程正式启动，且尚未输出统计报告
        this->isStressTesting = true;
        this->hasReportedMetrics = false;
    }
    lastPState = currentPState;

    // ====================================================================
    // 3. 受控于 GUI 看板的工业级 WMS 自动接单流水线
    // ====================================================================
    static int lastOrderGenerateTick = 0;
    static bool lastWmsState = false;

    bool currentWmsState = gui.Get_isWmsAutoReceiving();

    // 步骤 3.1：捕捉鼠标点击开启自动接单的上升沿，执行时间平移以消除初次触发的假死延时感
    if (currentWmsState && !lastWmsState) {
        lastOrderGenerateTick = globalSystemTick - 120;
    }
    lastWmsState = currentWmsState;

    // 步骤 3.2：进入 WMS 周期性接单流水线核心
    if (currentWmsState) {
        std::vector<int> validShelfIds;
        for (const auto& pair : stations) {
            if (pair.first != 999) validShelfIds.push_back(pair.first);
        }

        // 步骤 3.2.1：判定离散时间步 CD 是否冷却完毕 (每隔 60 步投放一笔真实电商任务)
        if (globalSystemTick - lastOrderGenerateTick >= 60) {
            if (!validShelfIds.empty()) {
                orderSystem.generateRandomOrder(validShelfIds);
                lastOrderGenerateTick = globalSystemTick; // 刚性推进冷却时间戳

                std::cout << "[WMS流水线] 成功接收一笔上游电商订单！当前系统时间: " << globalSystemTick << std::endl;
            }
        }
    }

    // ==================================================
    // 4. 时空 A* 寻路限流总线控制与多车动力动画更新
    // ==================================================
    // 步骤 4.1：单帧时空寻路限流阀状态复位，每帧滚动更新积压订单的饥饿感知指数（等待时间递增）
    bool pathCalculatedThisFrame = false;
    orderSystem.updateOrderTicks();

    // --------------------------------------------------
    // 运筹调度分配层：基于匈牙利算法的全局集中匹配
    // --------------------------------------------------
    if (!pathCalculatedThisFrame) {

        // 步骤 4.2：抓取当前真正具备承接任务资格的空闲小车（状态为 IDLE 且无路径负载）
        std::vector<Robot*> freeRobots;
        for (auto& r : robots) {
            if (r.status == RobotStatus::IDLE && r.pathQueue.empty()) {
                freeRobots.push_back(&r);
            }
        }

        // 步骤 4.3：抓取订单大盘中处于等待分配状态的活跃订单
        std::vector<Order*> waitingOrders;
        for (auto& o : orderSystem.getNonConstActiveOrders()) {
            if (o.status == OrderStatus::WAITING) {
                waitingOrders.push_back(&o);
            }
        }

        // 步骤 4.4：只有当“车源充足”且“任务积压”时，才正式激活匈牙利数学矩阵进行集中优化计算
        if (!freeRobots.empty() && !waitingOrders.empty()) {
            int nRows = (int)freeRobots.size();
            int nCols = (int)waitingOrders.size();
            std::vector<std::vector<int>> costMatrix(nRows, std::vector<int>(nCols, 0));

            const int TIME_WEIGHT = 2; // 时间防饥饿项的权重惩罚因子

            // 步骤 4.4.1：构建二维综合“代价矩阵”，平滑结合小车当前由于刚送完货所处的实时空间坐标
            for (int i = 0; i < nRows; ++i) {
                for (int j = 0; j < nCols; ++j) {
                    int stationId = waitingOrders[j]->targetStationId;
                    Station targetStation = this->getStationById(stationId);
                    Point dockPos = targetStation.dockPoint;

                    // 利用四舍五入平滑机制精准解算出智能体当前物理位姿对应的离散网格坐标
                    int curX = static_cast<int>(freeRobots[i]->realX + 0.5f);
                    int curY = static_cast<int>(freeRobots[i]->realY + 0.5f);
                    int manhattanDist = abs(curX - dockPos.x) + abs(curY - dockPos.y);

                    // 运用痛苦指数平衡公式：综合成本 = 曼哈顿距离代价 - 饥饿惩罚项
                    int cost = manhattanDist * 10 - (waitingOrders[j]->waitTicks / TIME_WEIGHT);
                    costMatrix[i][j] = (cost > 0) ? cost : 0; // 算法刚性防负底线拦截
                }
            }

            // 步骤 4.4.2：召唤离散数学矩阵求解器进行最优效益二分图匹配
            HungarianOptimizer optimizer;
            std::vector<int> assignment;
            optimizer.solve(costMatrix, assignment);

            // 步骤 4.4.3：遍历匹配决策结果，执行全局刚性单笔分发
            for (int i = 0; i < nRows; ++i) {
                int assignedOrderIdx = assignment[i];
                if (assignedOrderIdx != -1 && assignedOrderIdx < nCols) {
                    Robot* robot = freeRobots[i];
                    Order* order = waitingOrders[assignedOrderIdx];

                    // 步骤 4.4.4：双重寻路限制拦截，确保单帧内仅执行一辆小车的高耗时路由规划，保护核心推演帧率
                    if (!pathCalculatedThisFrame) {
                        order->status = OrderStatus::PROCESSING;
                        robot->status = RobotStatus::MOVING_TO_PICK;
                        robot->currentTargetStationId = order->targetStationId;
                        robot->currentOrderId = order->orderId;

                        // 核心进化：直接从小车当前所处的任何区域（包括侧道排队撤离点）发起就近时空寻路！
                        dispatchRobot(robot->id, order->targetStationId);

                        pathCalculatedThisFrame = true;
                        break;
                    }
                }
            }
        }
    }

    // 步骤 4.5：高频驱动小车物理实体执行空间坐标的平滑插值动画，并捕获逻辑步进信号
    bool anyRobotLogicStepped = false;
    bool anyRobotWorking = false;

    for (auto& r : robots) {
        r.update(globalTable, warehouseMap, systemTick);
        if (r.cellStepCompleted) anyRobotLogicStepped = true;
        if (r.status == RobotStatus::LOADING || r.status == RobotStatus::UNLOADING || r.status == RobotStatus::RETURNING_BUFFER) {
            anyRobotWorking = true;
        }
    }

    // ====================================================================
    // 5. 自动化物流双轨硬核传送带状态机闭环驱动
    // ====================================================================
    for (auto& r : robots) {

        // ---------- 阶段一：去货架取货与装载状态切换 ----------
        if (r.status == RobotStatus::MOVING_TO_PICK && r.pathQueue.empty() && (r.cellStepCompleted || r.moveProgress == 0.0f)) {
            r.status = RobotStatus::LOADING;
            r.workCooldown = 30; // 注入 30 步长的取货货架对位装载机械操作耗时
        }
        else if (r.status == RobotStatus::LOADING) {
            if (r.workCooldown > 0) {
                r.workCooldown--;
            }
            else {
                // 装载冷却结束，状态机切向送货阶段，指派路径前往出货口主直道排队闸门 (15, 10)
                r.status = RobotStatus::MOVING_TO_DELIVER;
                std::vector<Point> toGatePath = Router::getPath(r.currentPos, Point(15, 10), warehouseMap, globalTable, r.id, systemTick, false);
                r.setPath(toGatePath);
            }
        }

        // ---------- 阶段二：送货路径及门禁靠泊递补排队 ----------
        else if (r.status == RobotStatus::MOVING_TO_DELIVER) {

            // 步骤 5.1：【触头检测判定】：只要物理实体踩到排队道最前方的卸货靠泊格 (18, 10) 且路径排空，立刻强转卸货结算状态
            if (r.currentPos == Point(18, 10) && r.pathQueue.empty()) {
                r.status = RobotStatus::UNLOADING;
                r.workCooldown = 40; // 注入 40 步长的滚筒传送带卸货机械作业耗时
                continue;
            }

            // 步骤 5.2：【刚性逐格看前车递补机制】：针对卡在直道排队缓冲区（15~17, 10）且失去了路径的后车执行递进控制
            if (r.currentPos.y == 10 && r.currentPos.x >= 15 && r.currentPos.x <= 17 && r.pathQueue.empty()) {
                int nextX = r.currentPos.x + 1; // 测算前方的紧邻格离散坐标
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

                // 步骤 5.2.1：若前方格确认无车占位且无后勤锁锁闭，为后车注入向右靠拢的单格补位骨架路径
                if (!nextCellOccupied) {
                    std::vector<Point> stepForwardPath;
                    stepForwardPath.push_back(Point(nextX, 10));
                    r.pathQueue.clear();
                    r.setPath(stepForwardPath);
                    r.moveProgress = 0.0f;
                    r.cellStepCompleted = false;
                }
            }

            // 步骤 5.3：【中途拥堵空间冲突导致轨迹失效的重寻路机制】：完美收容处于大地图中、因资源竞争导致路径为空的小车
            else if (r.pathQueue.empty()) {
                // 严格遵守单帧时空 A* 寻路限流阀，绝对不抢占或挤爆 CPU 算力
                if (!pathCalculatedThisFrame) {
                    // 原地重新向主闸门入口 (15, 10) 发起时空避障寻路请求
                    std::vector<Point> toGatePath = Router::getPath(r.currentPos, Point(15, 10), warehouseMap, globalTable, r.id, systemTick, false);

                    if (!toGatePath.empty()) {
                        r.setPath(toGatePath); // 动态重寻路成功，重新注入移动轨迹！
                    }

                    // 绝杀安全控制：无论这次重寻路是成功还是由于时空极端拥堵再次失败，本帧的寻路限流阀都必须强行锁死，保护帧率！
                    pathCalculatedThisFrame = true;
                }
            }
        }

        // ---------- 阶段三：队头正忙，开始卸货结算 ----------
        else if (r.status == RobotStatus::UNLOADING) {

            if (r.workCooldown > 0) {
                r.workCooldown--;
            }
            else {
                // 步骤 5.4：在订单被彻底销毁和剔除前，先通过 ID 捞出非 const 订单引用，核算整体履约时间步并计入大盘数据
                if (r.currentOrderId != -1) {
                    const Order& finishedOrder = *orderSystem.getOrderById(r.currentOrderId);
                    this->onOrderFinished(finishedOrder);
                }

                // 步骤 5.5：切换状态至撤离线，正式触发 WMS 系统订单删除，并注入刚性侧边撤离骨架路径（直接切离核心主干道）
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
                // 步骤 5.6：【超级进化】：到达安全区释放点后，原地直接转为 IDLE 待命状态，但绝对不盲目规划回大本营的长路径！
                // 让它以无路径负载状态进入下一帧，直接接受匈牙利算法的全局扫描，实现真正的无缝就近运筹接单
                r.status = RobotStatus::IDLE;
                r.currentTargetStationId = -1;
            }
        }

        // ---------- 阶段五：业务兜底：防闲置智能体长期挂机回巢机制 ----------
        else if (r.status == RobotStatus::IDLE && r.pathQueue.empty()) {
            // 如果小车经历过全局匈牙利优化层扫描后依然没有被分发任务（说明订单缓冲池完全排空）
            // 且小车此时并不在自己的出生大本营坐标上，这时候再触发“防挂机自动回巢”安全机制
            Point homePos = { 0, 7 + r.id };
            if (r.currentPos.x != homePos.x || r.currentPos.y != homePos.y) {
                Point homeGate = { 1, 7 + r.id };
                std::vector<Point> toHomePath = Router::getPath(r.currentPos, homeGate, warehouseMap, globalTable, r.id, systemTick, false);
                toHomePath.push_back(homePos);
                r.setPath(toHomePath);
            }
        }
    }

    // ====================================================================
    // 6. 系统推演主时钟脉冲控制与压力测试全盘清算输出
    // ====================================================================
    // 步骤 6.1：时钟步进脉冲控制，只要有车在移动或处于装卸货作业中，全局时钟滚进
    if (anyRobotLogicStepped || anyRobotWorking) {
        systemTick++;
        this->globalSystemTick++;
    }

    // 步骤 6.2：检测 20 个并发高压测试订单全部顺利清盘，锁闭状态旗帜，刚性输出大盘对比看板
    if (this->isStressTesting && !this->hasReportedMetrics && this->totalCompletedOrders == 20) {

        this->hasReportedMetrics = true;
        this->isStressTesting = false;

        // 调用 WMS 核心物流数学公式解算三大核心运营 KPI 指标
        LogisticsMetrics res = this->calculateMetrics();

        // 步骤 6.2.1：在控制台终端刷出极其工整的工业级标准化账目报告
        std::cout << "\n==================================================" << std::endl;
        std::cout << "   [压力测试全盘清算报告 - 匈牙利算法优化组]" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << " 核心时钟 [大盘清盘总耗时] : " << this->globalSystemTick << " Ticks" << std::endl;
        std::cout << " 物流效率 [综合空驶率]     : " << res.emptyRunningRate << "%" << std::endl;
        std::cout << " 延时监控 [平均结单耗时]   : " << res.avgOrderCompletionTime << " Ticks" << std::endl;
        std::cout << " 吞吐效能 [系统单位吞吐]   : " << res.throughputPerKGrid << " 单/K-Ticks" << std::endl;
        std::cout << "==================================================\n" << std::endl;

        // 步骤 6.2.2：格式化动态拼装数据字符串，刚性注入 GUI 弹窗队列进行渲染呈现
        char str1[128], str2[128], str3[128];
        sprintf_s(str1, "[压测报告] 综合空驶率: %.1f%%", res.emptyRunningRate);
        sprintf_s(str2, "[性能监控] 平均结单耗时: %.1f Ticks", res.avgOrderCompletionTime);
        sprintf_s(str3, "[吞吐效能] 系统当前吞吐: %.2f 单/K-Ticks", res.throughputPerKGrid);

        gui.addPopup(str1, 15.0f);
        gui.addPopup(str2, 15.0f);
        gui.addPopup(str3, 15.0f);
    }
}

/**
 * @brief 基于匈牙利算法的上帝视角全局集中式调度核心优化函数
 * @param 无
 * @retval 无
 * @note 通过构建二分图效益匹配数学模型，全面打破局部贪婪分配的短视性，结合车辆实时位置与订单饥饿感知指数公式实现系统整体物流吞吐率的最优解。
 */
void WarehouseManager::dispatchOrdersByHungarian() {
    // 步骤 1.1：滚动系统中全部积压活跃订单的等待时钟
    orderSystem.updateOrderTicks();

    // 步骤 1.2：搜集运行时动态快照，挑出场上所有处于空闲、回巢待命状态的 AGV 指针实体
    std::vector<Robot*> freeRobots;
    for (auto& r : robots) {
        if (r.status == RobotStatus::IDLE) {
            freeRobots.push_back(&r);
        }
    }

    // 步骤 1.3：挑出订单管理系统中处于等待分配序列（WAITING）的非 const 活跃订单指针
    std::vector<Order*> waitingOrders;
    for (auto& o : orderSystem.getNonConstActiveOrders()) {
        if (o.status == OrderStatus::WAITING) {
            waitingOrders.push_back(&o);
        }
    }

    // 步骤 1.4：防御性边界保护：如果场上无空闲车，或无挂账订单，直接退出函数拦截无意义的二分图算力消耗
    if (freeRobots.empty() || waitingOrders.empty()) {
        return;
    }

    // 步骤 1.5：开始构建二维综合“代价匹配决策矩阵” (行标: 空闲小车, 列标: 等待订单)
    int nRows = (int)freeRobots.size();
    int nCols = (int)waitingOrders.size();
    std::vector<std::vector<int>> costMatrix(nRows, std::vector<int>(nCols, 0));

    const int TIME_WEIGHT = 2; // 设定时间防饥饿项的惩罚项权重因子

    for (int i = 0; i < nRows; ++i) {
        for (int j = 0; j < nCols; ++j) {
            // 步骤 1.5.1：拉取特定订单的目的地货架站点拓扑坐标并抽取出对应的靠泊交互网格
            int stationId = waitingOrders[j]->targetStationId;
            Station targetStation = this->getStationById(stationId);
            Point dockPos = targetStation.dockPoint;

            // 步骤 1.5.2：基于四舍五入法平滑计算小车因刚执行完上一笔订单或在撤离区所处的实时网格坐标
            int curX = static_cast<int>(freeRobots[i]->realX + 0.5f);
            int curY = static_cast<int>(freeRobots[i]->realY + 0.5f);
            int manhattanDist = abs(curX - dockPos.x) + abs(curY - dockPos.y);

            // 步骤 1.5.3：运用痛苦指数公式：综合代价 = 实时位置曼哈顿距离成本 - 积压等待带来的饥饿惩罚项
            int cost = manhattanDist * 10 - (waitingOrders[j]->waitTicks / TIME_WEIGHT);

            // 匈牙利优化算法要求输入代价绝对不能为负数，执行零基准底层安全防御
            costMatrix[i][j] = (cost > 0) ? cost : 0;
        }
    }

    // 步骤 1.6：调用匈牙利矩阵核心求解器，开启离散数学优化逻辑
    HungarianOptimizer optimizer;
    std::vector<int> assignment; // 存储第 i 辆车分到的最合理订单索引映像
    optimizer.solve(costMatrix, assignment);

    // 步骤 1.7：解析离散最优匹配结果矩阵，开始执行全局刚性批量任务分配下发
    for (int i = 0; i < nRows; ++i) {
        int assignedOrderIdx = assignment[i];

        // 若这辆空闲 AGV 成功匹配到了一个真实的未履约订单（非优化补齐虚拟节点）
        if (assignedOrderIdx != -1 && assignedOrderIdx < nCols) {
            Robot* robot = freeRobots[i];
            Order* order = waitingOrders[assignedOrderIdx];

            // 步骤 1.7.1：将上游订单调度状态强切为执行中（PROCESSING）
            order->status = OrderStatus::PROCESSING;

            // 步骤 1.7.2：刚性绑定该小车的业务上下文，注入业务执行核心数据
            robot->status = RobotStatus::MOVING_TO_PICK; // 智能体状态切换：全速前往货架取货
            robot->currentOrderId = order->orderId;
            robot->currentTargetStationId = order->targetStationId;

            // 步骤 1.7.3：驱动底层 RCS 动力寻路马达，触发首段路径的时空轨迹生成
            this->dispatchRobot(robot->id, order->targetStationId);
        }
    }
}

/**
 * @brief 订单诞生事件同步钩子接口
 * @param order 刚被系统创建生成的订单对象引用
 * @retval 无
 * @note 赋予订单诞生的全局系统绝对时间戳，作为后续衡量其履约周期与综合时延的核心基准线。
 */
void WarehouseManager::onOrderCreated(Order& order) {
    order.createTick = this->globalSystemTick;
}

/**
 * @brief 订单履约完成事件综合结算钩子接口
 * @param order 刚刚在出货口完成卸货清盘的订单常量引用
 * @retval 无
 * @note 原子性地累加系统总吞吐完单量，并将该订单的生存生命周期精准并入全局耗时大盘，用于延时效能评估。
 */
void WarehouseManager::onOrderFinished(const Order& order) {
    this->totalCompletedOrders++;
    int duration = this->globalSystemTick - order.createTick; // 当前绝对时钟减去诞生时钟
    this->totalOrderDuration += duration;
}

/**
 * @brief 智能仓储三大核心物流效能指标（KPI）动态综合解算函数
 * @param 无
 * @retval LogisticsMetrics 包含综合空驶率、平均履约时延和单位吞吐效能的复合数据结构
 * @note 配备鲁棒的防除零保护机制，用于实时向系统上层输出最真实的数字化精细运营统计报告。
 */
WarehouseManager::LogisticsMetrics WarehouseManager::calculateMetrics() {
    LogisticsMetrics metrics{ 0.0, 0.0, 0.0 };

    // 步骤 1.1：聚合集群所有移动机器人的总行驶里程与空驶里程，精准解算全盘综合空驶率
    int aggregateTotalGrids = 0;
    int aggregateEmptyGrids = 0;
    for (const auto& robot : robots) {
        aggregateTotalGrids += robot.totalGridsWalked;
        aggregateEmptyGrids += robot.emptyGridsWalked;
    }
    if (aggregateTotalGrids > 0) {
        metrics.emptyRunningRate = (static_cast<double>(aggregateEmptyGrids) / aggregateTotalGrids) * 100.0;
    }

    // 步骤 1.2：基于当前完单总量，执行单仓订单平均履约周期的浮点解耦解算
    if (totalCompletedOrders > 0) {
        metrics.avgOrderCompletionTime = static_cast<double>(totalOrderDuration) / totalCompletedOrders;
    }

    // 步骤 1.3：标准化消除时间尺度差异，解算出每 1000 Ticks 系统承受的净吞吐速率表现
    if (globalSystemTick > 0) {
        metrics.throughputPerKGrid = (static_cast<double>(totalCompletedOrders) * 1000.0) / globalSystemTick;
    }

    std::cout << "[Debug 看板数据] 总完单: " << totalCompletedOrders
        << " | 总耗时: " << totalOrderDuration
        << " | WMS当前时钟: " << globalSystemTick << std::endl;

    return metrics;
}

/**
 * @brief 强制调度指定机器人前往特定离散网格坐标（人工高级干预与调试高危接口）
 * @param robotId 目标强驱的移动机器人全局唯一识别编号
 * @param targetX 强驱目的地的离散网格横轴坐标 X
 * @param targetY 强驱目的地的离散网格纵轴坐标 Y
 * @retval 无
 * @note 强行中断 AGV 原有作业任务链并释放时空预订，利用唯一 ID 安全检索机制，防止因向量底层地址动态漂移引发越界及非法指针崩溃。
 */
void WarehouseManager::moveRobotToGrid(int robotId, int targetX, int targetY) {
    // 步骤 1.1：根据真实的机器人全局 ID 遍历检索其在容器中的内存地址，绝对不盲目相信数组下标
    Robot* targetRobot = nullptr;
    for (auto& r : this->robots) {
        if (r.id == robotId) {
            targetRobot = &r;
            break;
        }
    }

    // 步骤 1.2：防御性指针空置防护校验，全面拦截由于外部传入非法 ID 导致核心推演线程瞬时崩溃的高危场景
    if (targetRobot == nullptr) {
        std::cout << "[ERROR] 强驱失败：未找到 ID 为 " << robotId << " 的机器人！" << std::endl;
        return;
    }

    // 步骤 2.1：强行清空小车当前残余路径队列，平滑计算其当前物理位姿对应的离散网格取整起点
    targetRobot->pathQueue.clear();
    Point startPos(static_cast<int>(targetRobot->realX + 0.5f), static_cast<int>(targetRobot->realY + 0.5f));

    // 步骤 3.1：拉取底层时空 A* 路由器计算安全无冲突的最优干预轨迹（robotId 传入 Router 内部作为碰撞豁免标识）
    std::vector<Point> path = Router::getPath(startPos, Point(targetX, targetY), warehouseMap, globalTable, robotId, systemTick, false);

    // 步骤 4.1：将生成的干预轨迹正确注入目标小车，并将状态强切至 IDLE，以供后续重新切回 WMS 电商订单自动化大盘调度
    targetRobot->setPath(path);
    targetRobot->status = RobotStatus::IDLE;
}