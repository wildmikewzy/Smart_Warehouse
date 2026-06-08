/*核心的算法接口*/
#include "Router.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <climits>
#include <string>
#include <iostream>

using namespace std;

/**
 * @brief 内部辅助的时空搜索节点结构体
 * @note 扩展了传统 A* 算法的二维空间维度，引入时间轴 t 作为第三维度，构筑三维时空寻路状态空间。
 */
struct STNode {
    Point pt; // 当前网格的空间拓扑坐标 (x, y)
    int t;    // 当前状态所对应的系统绝对离散时间戳
    int g;    // 从规划起点到当前时空状态点的真实累积路径代价
    int h;    // 当前时空点到目标空间终点的启发式预估代价

    /**
     * @brief 计算当前节点的总评估代价 f(n) = g(n) + h(n)
     * @retval int 综合时空代价评估值
     */
    int f() const { return g + h; }

    /**
     * @brief 重载大于运算符，用于优先队列（小顶堆）的降序排列
     * @param other 对比的另一个时空节点
     * @retval bool 若当前节点总代价大于目标节点，返回 true，确保低代价节点优先出堆
     */
    bool operator>(const STNode& other) const {
        return f() > other.f();
    }
};

/**
 * @brief 曼哈顿距离启发式估算函数
 * @param a 起始网格坐标点
 * @param b 目标网格坐标点
 * @retval int 规范化的曼哈顿物理距离
 * @note 在严格十字轴向（非斜行）的仓储网格地图中，曼哈顿距离满足绝对可采性（Admissible）与单调性（Consistent），能确保 A* 算法求解出全局最优解。
 */
static int heuristic(const Point& a, const Point& b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

/**
 * @brief 生成时空边冲突校验的唯一标识特征键（针对外层兼容性的历史遗留接口）
 * @param x1 移动起始点的物理横轴坐标 X
 * @param y1 移动起始点的物理纵轴坐标 Y
 * @param x2 移动目标点的物理横轴坐标 X
 * @param y2 移动目标点的物理纵轴坐标 Y
 * @param t 触发该移动行为的离散时间戳刻度
 * @retval string 格式化的时空边唯一关联特征字符串键值
 * @note 专门用于锁闭和查询具有时空排他性的图边资源，防止智能体在相邻时间步长发生空间对穿碰撞。
 */
static string getEdgeKey(int x1, int y1, int x2, int y2, int t) {
    return to_string(x1) + "," + to_string(y1) + "->" + to_string(x2) + "," + to_string(y2) + "@" + to_string(t);
}

/**
 * @brief 动态时空安全性防护与碰撞冲突拦截校验检测函数
 * @param currX 当前智能体所处的网格横轴坐标 X
 * @param currY 当前智能体所处的网格纵轴坐标 Y
 * @param currT 当前智能体所处状态的绝对时间戳
 * @param px 探针前瞻预测的下一步空间横轴目标坐标 X
 * @param py 探针前瞻预测的下一步空间纵轴目标坐标 Y
 * @param nextT 探针前瞻预测的下一步绝对时间戳 (通常为 currT + 1)
 * @param robotId 当前发起路由寻路请求的智能体全局唯一识别编号
 * @param table 全局时空资源预订快照表（Reservation Table）的常量引用
 * @retval bool 若目标时空位置安全无冲突返回 true；若存在空间竞争或对穿风险则返回 false。
 * @note 核心算法涵盖了两大 MAPF 安全防御机制：空间顶点独占冲突（Vertex Collision）与时间步对穿边冲突（Edge Swap Collision）。
 */
static bool isTimeSpaceSafe(int currX, int currY, int currT, int px, int py, int nextT, int robotId, const ReservationTable& table) {
    // ----------------------------------------------------------------
    // 1. 空间顶点冲突校验（Vertex Collision Check）
    // ----------------------------------------------------------------
    // 步骤 1.1：精准点冲突检查。判定在目标时间步 nextT 瞬间，目标空间网格 (px, py) 是否已被其他智能体预订
    TimePoint targetCheck = { px, py, nextT };
    auto it = table.vertexReservations.find(targetCheck);
    if (it != table.vertexReservations.end() && it->second != robotId) {
        return false; // 该时空格子专属所有权归属于其他作业车辆，触发顶点冲突拦截
    }

    // 步骤 1.2：高动态前瞻防撞冗余检查。判定对方是否会在紧邻的下一帧（nextT + 1）强制突入此网格，提前切断交叉碰撞隐患
    TimePoint futureCheck = { px, py, nextT + 1 };
    auto itFuture = table.vertexReservations.find(futureCheck);
    if (itFuture != table.vertexReservations.end() && itFuture->second != robotId) {
        return false; // 探测到未来时间帧存在潜入冲突风险，执行防御性降级拦截
    }

    // ----------------------------------------------------------------
    // 2. 时空边冲突校验（Edge Swap Collision Check）
    // ----------------------------------------------------------------
    // 步骤 2.1：对穿冲突碰撞锁闭检查。判定是否存在另一个智能体在相同的离散时钟周期（currT）内，做相反方向的拓扑位置互换。
    // 即：对方在 currT 从 (px, py) 突入 (currX, currY)。
    string swapEdgeKey = getEdgeKey(px, py, currX, currY, currT);
    if (table.edgeReservations.count(swapEdgeKey) > 0) {
        return false; // 探测到两车时空轨迹发生绝对对穿，强行拦截
    }

    return true; // 经多维度联合时空测算，判定该移动步长安全合规
}

/**
 * @brief 三维时空状态高密压缩编码函数（状态空间底层极速优化算法）
 * @param x 映射的空间网格横轴坐标 X
 * @param y 映射的空间网格纵轴坐标 Y
 * @param t 映射的离散时间轴步长刻度 t
 * @retval uint64_t 编码压缩后的 64 位无符号整型全局唯一状态主键
 * @note 通过位移运算（Bit-shifting）将三维时空状态（8位存X，8位存Y，32位存时间T）高密打包，
 * 彻底消除 A* 搜索过程中高频构建 `std::string` 或复合结构体所导致的频繁无用动态内存分配与堆内存碎片化问题，极大提升哈希检索性能。
 */
static inline uint64_t buildStateKey(int x, int y, int t) {
    // 步骤 1.1：利用位掩码限制空间边界，高 8 位存放 X 轴坐标，次高 8 位存放 Y 轴坐标，低 32 位存放绝对时间戳 t
    uint64_t key = (static_cast<uint64_t>(x) & 0xFF) << 40;
    key |= (static_cast<uint64_t>(y) & 0xFF) << 32;
    key |= (static_cast<uint64_t>(t) & 0xFFFFFFFF);
    return key;
}

/**
 * @brief 多智能体时空 A* 路径寻路核心算法接口
 * @param start 寻路规划的起点空间物理坐标
 * @param end 寻路规划的终点空间物理坐标
 * @param map 仓储静态网格地图障碍物看板的常量引用
 * @param table 全局时空资源预订锁闭大盘的常量引用
 * @param robotId 发起当前路由寻路指令的移动机器人全局唯一识别 ID
 * @param startTime 当前寻路决策被触发时的全局系统起始时钟 Tick
 * @param stopAdjacent 是否执行邻格靠泊终止优化配置（布尔旗帜）
 * @retval std::vector<Point> 规划解算出的绝对零冲突、平滑无交织的时空拓扑路径节点队列
 * @note 工业级 RCS 核心底座算法，内嵌主直道排队空间物理隔离机制、出货撤离道（Y=9）单向时空流控机制、以及面向平滑动力学的转弯惩罚权重算子。
 */
vector<Point> Router::getPath(Point start, Point end, const Map& map,
    const ReservationTable& table, int robotId,
    int startTime, bool stopAdjacent) {
    vector<Point> path;

    // ====================================================================
    // 1. 静态边界安全前置拦截与边界防护
    // ====================================================================
    // 步骤 1.1：校验空间起点与目标终点在静态地图网格中是否属于可通行空地，若踩中货架等死格立刻拦截
    if (!map.isWalkable(start.x, start.y) || !map.isWalkable(end.x, end.y)) return path;

    // 步骤 1.2：若起点与终点完全重合，原地生成单节点骨架路径，安全中断后续算法耗时
    if (start.x == end.x && start.y == end.y) {
        path.push_back(start);
        return path;
    }

    // ====================================================================
    // 2. 离散时空 A* 数据结构初始化与时空维度的开辟
    // ====================================================================
    // 步骤 2.1：构建高性能无符号 64 位空间主键映射表，全面替代庞大的三维状态哈希
    unordered_map<uint64_t, int> g;
    unordered_map<uint64_t, STNode> parent;
    priority_queue<STNode, vector<STNode>, greater<STNode>> pq;

    // 步骤 2.2：高密编码初始化起点时空状态，注入大盘起始系统时钟（startTime），计算初始曼哈顿预估距离
    uint64_t startKey = buildStateKey(start.x, start.y, startTime);
    g[startKey] = 0;
    int h_Start = heuristic(start, end);
    pq.push({ start, startTime, 0, h_Start });

    // 步骤 2.3：声明离散数学五方向移动转移算子（包含：东、西、南、北四个空间物理位移方向 + 原地时空等待）
    int dx[] = { 0, 0, 1, -1, 0 };
    int dy[] = { 1, -1, 0, 0, 0 };

    bool found = false;
    STNode endNode;

    // 步骤 2.4：自适应动态时空前瞻搜索窗口深度配置
    int maxTimeWindow = startTime + 100; // 默认前瞻 100 步时空深度限制
    if (end.x == 18 && end.y == 10) {
        maxTimeWindow = startTime + 500; // 目标属于核心出货口（OUT站）时，大幅放宽搜索时空容忍深度，允许智能体执行超长时延的排队等待
    }

    // ====================================================================
    // 3. 时空 A* 核心图搜索迭代主循环
    // ====================================================================
    while (!pq.empty()) {
        // 步骤 3.1：提取并弹出当前时空代价评估模型最低的节点实体
        STNode current = pq.top();
        pq.pop();

        // 步骤 3.2：空间终点到达判定。若当前时空节点的空间坐标匹配目标终点，宣告寻路成功并终止迭代
        if (current.pt == end) {
            found = true;
            endNode = current;
            break;
        }

        // 步骤 3.3：时空深度边界保护。若当前状态的绝对时间刻度超出了动态安全时空窗口，则抛弃此分支的深度延展
        if (current.t >= maxTimeWindow) continue;

        // 步骤 3.4：哈希剪枝。若当前出堆节点的真实开销 g 已经劣于历史记录中的更优解，执行过时节点抛弃
        uint64_t currKey = buildStateKey(current.pt.x, current.pt.y, current.t);
        if (current.g > g[currKey]) continue;

        // 步骤 3.5：遍历五方向状态转移方程，测算子代状态空间的安全合规性
        for (int i = 0; i < 5; i++) {
            int px = current.pt.x + dx[i];
            int py = current.pt.y + dy[i];
            int nextT = current.t + 1; // 时间轴随着状态推进单向无逆滚动增长

            // 步骤 3.5.1：静态地图物理边界与合法性拦截
            if (px < 0 || px >= MAP_LENGTH || py < 0 || py >= MAP_WIDTH) continue;
            if (!map.isWalkable(px, py)) continue;

            // =================================================================
            // 🌟 核心控制层：出货口直道排队核心缓冲区物理安全隔离机制 (16~18, 10)
            // =================================================================
            // 业务设计：当非送货、非执行排队任务的通用出勤车辆企图以 (15, 10) 作为寻路终点或路径跳板时，
            // 必须刚性切断其穿越排队直道核心占用区的可能性，将该片物理空间让给正在进行传送带对接的卸货车。
            if (end.x == 15 && end.y == 10) {
                if (py == 10 && px >= 16 && px <= 18) {
                    continue; // 触发空间裁剪，禁止借道横穿核心排队直道区
                }
            }

            // =================================================================
            // 🌟 核心控制层：卸货撤离通道单向时空流控机制 (15~18, 9)
            // =================================================================
            // 当 A* 算法向前探针测算出下一步企图突入或途经 Y = 9 的单向专属撤离通道时：
            if (py == 9 && px >= 15 && px <= 18) {

                // 流控铁律一：动态逆向行驶拦截。
                // 卸货撤离线在交通流拓扑上被规定为单向由右向左，若前瞻横轴坐标 px 大于当前空间坐标，
                // 说明路径正在执行向右逆行，直接予以剪枝拦截，防范对向死锁。
                if (px > current.pt.x) {
                    continue;
                }

                // 流控铁律二：非离场作业车辆纵向长距离借道裁剪。
                // 如果当前发起寻路的目标并非刚从 (18, 10) 完成卸货出来的离场车（即属于大仓内部其他取货车辆临时穿行）：
                if (!(start.x == 18 && start.y == 10)) {
                    // 允许其垂直南北穿过撤离通道，但一旦发现其企图在 Y=9 通道内执行长距离的横向尾随或并线挂账，
                    // 为了保护主离场线的极致通畅度，立刻予以剪枝劝退。
                    if (current.pt.y == 9 && px != current.pt.x) {
                        continue;
                    }
                }
            }
            // =================================================================

            // 步骤 3.5.2：调用动态时空防撞内核，检索 Reservation 表进行多智能体点边碰撞交叉判定
            if (!isTimeSpaceSafe(current.pt.x, current.pt.y, current.t, px, py, nextT, robotId, table)) {
                continue; // 存在时空资源冲突，剪枝该分支
            }

            // ====================================================================
            // 4. 平滑动力学代价值计算与转向惩罚权重注入
            // ====================================================================
            int edgeCost = 1;     // 标准时间步长消耗代价基准
            int turnPenalty = 0;  // 转弯惩罚权重算子，全面杜绝锯齿状高频振荡航线生成

            if (i == 4) {
                edgeCost = 1; // 原地执行时空等待时的标准时钟耗时损耗
            }
            else {
                // 步骤 4.1：平滑转弯惩罚测算。通过回溯当前节点的父节点，提取出上一步的位移运动矢量（lastDx, lastDy）
                if (parent.find(currKey) != parent.end()) {
                    Point lastPt = parent[currKey].pt;
                    int lastDx = current.pt.x - lastPt.x;
                    int lastDy = current.pt.y - lastPt.y;
                    int currentDx = px - current.pt.x;
                    int currentDy = py - current.pt.y;

                    // 步骤 4.1.1：当判定运动矢量方向发生夹角改变（发生转弯），且上一步移动非原地等待时：
                    // 施加正向强惩罚代价（turnPenalty = 3），迫使 A* 寻路倾向于规划笔直的工业级高标线轨迹。
                    if ((lastDx != currentDx || lastDy != currentDy) && (lastDx != 0 || lastDy != 0)) {
                        turnPenalty = 3;
                    }
                }
            }

            // 步骤 4.2：计算全新的子节点真实开销值并执行堆结构松弛化注入
            int newG = current.g + edgeCost + turnPenalty;
            uint64_t nextKey = buildStateKey(px, py, nextT);

            if (g.find(nextKey) == g.end() || newG < g[nextKey]) {
                g[nextKey] = newG;
                parent[nextKey] = current;
                int h_Neighbor = heuristic({ px, py }, end);
                pq.push({ {px, py}, nextT, newG, h_Neighbor });
            }
        }
    }

    // ====================================================================
    // 5. 时空路径链表倒序回溯与物理路径提取
    // ====================================================================
    if (found) {
        STNode temp = endNode;
        // 步骤 5.1：利用哈希指针父子树关联，从终点时空节点逆向回溯直至起点
        while (true) {
            path.push_back(temp.pt);
            uint64_t tempKey = buildStateKey(temp.pt.x, temp.pt.y, temp.t);
            if (parent.find(tempKey) == parent.end()) break;
            temp = parent[tempKey];
        }
        // 步骤 5.2：由于是从尾部回溯，在此执行翻转，重组符合机器人正向时间轴的逻辑路径队列
        reverse(path.begin(), path.end());

        // 步骤 5.3：邻格靠泊终止空间裁剪。若开启邻格终止，则剥离掉最后一个完全跨进货架实体的点，停留在靠泊交互格。
        if (stopAdjacent && path.size() > 1) {
            path.pop_back();
        }
    }

    // 步骤 5.4：标准的工业控制终端日志输出，实时打印当前小车的规划时空物理路径
    cout << "小车路径打印：";
    for (auto& m : path) {
        cout << "(" << m.x << "," << m.y << ") ";
    }
    cout << endl;

    return path; // 返回无冲突的安全行驶轨迹队列
}