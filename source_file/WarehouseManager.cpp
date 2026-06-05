#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <windows.h>
#include "WarehouseManager.h"
#include "common.h"

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
    // 从最前方的 18 开始向左扫描到 15
    for (int x = 18; x >= 15; --x) {
        bool isOccupied = false;
        for (const auto& r : robots) {
            if (r.currentPos.y == 10 && r.currentPos.x == x) {
                isOccupied = true;
                break;
            }
            if (r.status == RobotStatus::MOVING_TO_DELIVER && !r.pathQueue.empty() && r.pathQueue.back().y == 10 && r.pathQueue.back().x == x) {
                isOccupied = true;
                break;
            }
        }
        if (!isOccupied) {
            return x; // 找到最靠前的黄金空位
        }
    }
    return 15; // 全满时保底停在大门
}
/**
 * @brief 系统主循环核心每帧逻辑驱动、状态机闭环及多车时间步强对齐核心函数
 * @param 无
 * @retval 无
 * @details 扮演仓储系统的 WMS 控制大脑与中央时钟源。每帧按顺序触发：
 */
void WarehouseManager::updateAll() {
    // ==========================================
    // 测试阶段：只生成 4 个初始订单
    // ==========================================
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
            ordersGenerated = true; // 锁死，目前只生成4个订单
        }
    }
    static bool lastTState = false; // 记录上一帧 T 键是否被按下
    bool currentTState = (GetAsyncKeyState('T') & 0x8000) != 0; // 当前帧 T 键状态

    // 只有当“上一帧没按”且“这一帧按了”时，才判定为真正按下（单次敲击）
    if (currentTState && !lastTState) {
        // 2. 重置生成开关，允许在下一帧精准触发 4 个新订单
        ordersGenerated = false;
    }

    lastTState = currentTState; // 更新按键历史状态，为下一帧做准备

    // ==================================================
    // 【核心新增控制】：单帧时空 A* 寻路限流阀
    // ==================================================
	bool pathCalculatedThisFrame = false;       //节流阀：本帧是否已经有车触发过寻路了

    // 1. 核心调度轮询：不断抓取等待中的订单分发给空闲车
    Order* currentOrder = orderSystem.getFirstWaitingOrder();
    // 🚨 【增加限制】：如果本帧还没有进行过寻路，才允许抓取并分发新订单
    if (currentOrder != nullptr && !pathCalculatedThisFrame) {
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

            // 触发高耗时时空 A* 寻路
            dispatchRobot(availableRobot->id, currentOrder->targetStationId);

            // 🚨 锁定限流阀：本帧寻路名额已用完，后续逻辑或其他车辆本帧严禁寻路！
            pathCalculatedThisFrame = true;
        }
    }

    // 2. 高频驱动小车肉体动画插值，并捕获步进脉冲信号
    bool anyRobotLogicStepped = false;
    bool anyRobotWorking = false; // 【新增防御线】：记录有没有车正在原地工作

    for (auto& r : robots) {
        r.update();
        if (r.cellStepCompleted) anyRobotLogicStepped = true;
        if (r.status == RobotStatus::LOADING || r.status == RobotStatus::UNLOADING || r.status == RobotStatus::RETURNING_BUFFER) {
            anyRobotWorking = true;
        }
    }
    // ==================================================
    // 3. 核心改进：双轨硬核传送带状态机
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
                // 装货结束！终点死死锁定为新大门 (15, 10)
                r.status = RobotStatus::MOVING_TO_DELIVER;
                std::vector<Point> toGatePath = Router::getPath(r.currentPos, Point(15, 10), warehouseMap, globalTable, r.id, systemTick, false);
                r.setPath(toGatePath);
            }
        }

        // ---------- 阶段二：送货路上与【新大门门禁动态排队】 ----------
        else if (r.status == RobotStatus::MOVING_TO_DELIVER) {

            // 当小车肉体真正踩到了新大门 (15, 10)，且去大门的路径已经走完
            if (r.currentPos == Point(15, 10) && r.pathQueue.empty()) {

                // 动态扫描 15~18，看看最前面哪个位置空着
                int realTargetX = getNextAvailableQueueX_AtGate(robots);

                // 如果前方有空位（即算出来的目标列大于大门 15）
                if (realTargetX > 15) {
                    std::vector<Point> queueStraightPath;
                    // 刚性驱动：从 16 开始硬写向右的直线路径，绝对不走 A* 寻路
                    for (int x = 16; x <= realTargetX; ++x) {
                        queueStraightPath.push_back(Point(x, 10));
                    }
                    r.setPath(queueStraightPath);
                }
                // 如果算出来就是 15 本身，说明前面全卡满了，小车就老老实实卡在 (15, 10) 大门口死等
            }
        }
    }
    // 4. 只要有小车走完一格，或者有小车在装卸货工作中，就推进全局系统时钟（逻辑步数）
    if (anyRobotLogicStepped || anyRobotWorking) {
        systemTick++;
    }
}