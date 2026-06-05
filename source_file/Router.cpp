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

// 内部辅助的时空搜索节点
struct STNode {
    Point pt;
    int t; // 当前时间戳
    int g; // 起点到目前的真实代价
    int h; // 启发值
    int f() const { return g + h; }

    bool operator>(const STNode& other) const {
        return f() > other.f();
    }
};

/**
* @brief 曼哈顿距离启发函数
*/
static int heuristic(const Point& a, const Point& b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

// 辅助函数：生成边占领的唯一Key (优化：依然保留string，但由于仅外层验证碰撞尚可接受)
static string getEdgeKey(int x1, int y1, int x2, int y2, int t) {
    return to_string(x1) + "," + to_string(y1) + "->" + to_string(x2) + "," + to_string(y2) + "@" + to_string(t);
}

// 时空安全检查：验证在 t 时刻移动到 (px, py) 是否会发生冲突
static bool isTimeSpaceSafe(int currX, int currY, int px, int py, int t, int robotId, const ReservationTable& table) {
    // 1. 点冲突检查（时间膨胀：t-1, t, t+1）
    for (int dt = -1; dt <= 1; ++dt) {
        int checkT = t + dt;
        if (checkT < 0) continue;
        TimePoint targetCheck = { px, py, checkT };
        auto it = table.vertexReservations.find(targetCheck);
        if (it != table.vertexReservations.end() && it->second != robotId) {
            return false;
        }
    }

    // 2. 边冲突检查（Edge Swap Collision）
    string swapEdgeKey = getEdgeKey(px, py, currX, currY, t);
    if (table.edgeReservations.count(swapEdgeKey) > 0) {
        return false;
    }

    return true;
}

// 优化：将三维(x,y,t)转化为 64 位无符号整数表示，彻底消灭 string 的内存分配灾难！
static inline uint64_t buildStateKey(int x, int y, int t) {
    uint64_t key = (static_cast<uint64_t>(x) & 0xFF) << 40;       // 8位存 x
    key |= (static_cast<uint64_t>(y) & 0xFF) << 32;               // 8位存 y
    key |= (static_cast<uint64_t>(t) & 0xFFFFFFFF);               // 32位存时间 t
    return key;
}

vector<Point> Router::getPath(Point start, Point end, const Map& map,
    const ReservationTable& table, int robotId,
    int startTime, bool stopAdjacent) {
    vector<Point> path;

    // 1. 基础合法性检查
    if (!map.isWalkable(start.x, start.y) || !map.isWalkable(end.x, end.y)) return path;

    if (start.x == end.x && start.y == end.y) {
        path.push_back(start);
        return path;
    }

    // 利用哈希表与整型主键极大缩小寻路中的检索与写入开销
    unordered_map<uint64_t, int> g;
    unordered_map<uint64_t, STNode> parent;
    priority_queue<STNode, vector<STNode>, greater<STNode>> pq;

    // 初始化起点
    uint64_t startKey = buildStateKey(start.x, start.y, startTime);
    g[startKey] = 0;
    int h_Start = heuristic(start, end);
    pq.push({ start, startTime, 0, h_Start });

    // 严格强制的十字轴向移动(不允许走斜线) + 原地等待
    int dx[] = { 0, 0, 1, -1, 0 };
    int dy[] = { 1, -1, 0, 0, 0 };

    bool found = false;
    STNode endNode;

    // A* 最高搜索深容忍防死环
    int maxTimeWindow = startTime + 100;
    if (end.x == 18 && end.y == 10) {
		maxTimeWindow = startTime + 500;    // 目标是出货口的时候，适当放宽搜索深度容忍度
    }

    while (!pq.empty()) {
        STNode current = pq.top();
        pq.pop();

        if (current.pt == end) {
            found = true;
            endNode = current;
            break;
        }

        if (current.t >= maxTimeWindow) continue; 

        uint64_t currKey = buildStateKey(current.pt.x, current.pt.y, current.t);
        if (current.g > g[currKey]) continue;

        for (int i = 0; i < 5; i++) {
            int px = current.pt.x + dx[i];
            int py = current.pt.y + dy[i];
            int nextT = current.t + 1; 

            if (px < 0 || px >= MAP_LENGTH || py < 0 || py >= MAP_WIDTH) continue;
            if (!map.isWalkable(px, py)) continue; // 坚决不能踩障碍物
            // =================================================================
            // 🌟 排队直道核心区隔离 (16~18, 10)
            // =================================================================
            if (end.x == 15 && end.y == 10) {
                if (py == 10 && px >= 16 && px <= 18) {
                    continue;
                }
            }

            // =================================================================
            // 🌟【完美闭环】：Y=9 返回道“单向平交道”空间保护 (15~18, 9)
            // =================================================================
            // 如果 A* 探测到下一步企图踩进或路过 Y = 9 的返回道核心区
            if (py == 9 && px >= 15 && px <= 18) {

                // 铁律一：任何人（不管是取货车还是离场车）在这条道上绝对禁止向右逆行！
                // 也就是说，如果下一步的 px 比当前点的 current.pt.x 还要大，说明在向右走，直接剪枝拦截
                if (px > current.pt.x) {
                    continue;
                }

                // 铁律二：如果是外面的车（去取货的车）想借道、穿过这条路
                if (!(start.x == 18 && start.y == 10)) {
                    // 如果它在 Y=9 上试图横向长距离霸占车道（例如从 18 走到 15），为了不影响离场效率，把它劝退
                    // 允许它垂直穿过（比如从 Y=10 穿到 Y=8，或者原地等待）
                    if (current.pt.y == 9 && px != current.pt.x) {
                        continue; // 禁止在返回道内做长距离的纵向尾随
                    }
                }
            }
            // =================================================================
            std::string reverseEdgeKey = to_string(px) + "," + to_string(py) + "->" +
                to_string(current.pt.x) + "," + to_string(current.pt.y) + "@" + to_string(nextT);
            if (table.edgeReservations.count(reverseEdgeKey) > 0) {
                continue;
            }

            if (!isTimeSpaceSafe(current.pt.x, current.pt.y, px, py, nextT, robotId, table)) {
                continue; 
            }

            int edgeCost = 1;
            int turnPenalty = 0;        //转弯惩罚，杜绝锯齿状路径

            if (i == 4) {
                edgeCost = 1; // 原地等待代价
            }
            else {
                if (parent.find(currKey) != parent.end()) {
                    Point lastPt = parent[currKey].pt;
                    int lastDx = current.pt.x - lastPt.x;
                    int lastDy = current.pt.y - lastPt.y;
                    int currentDx = px - current.pt.x;
                    int currentDy = py - current.pt.y;

                    // 当方向确实改变，且上一步不是原地等待，施加惩罚让路径尽量笔直不跳变
                    if ((lastDx != currentDx || lastDy != currentDy) && (lastDx != 0 || lastDy != 0)) {
                        turnPenalty = 3;
                    }
                }
            }

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

    // 回溯时空路径
    if (found) {
        STNode temp = endNode;
        while (true) {
            path.push_back(temp.pt);
            uint64_t tempKey = buildStateKey(temp.pt.x, temp.pt.y, temp.t);
            if (parent.find(tempKey) == parent.end()) break;
            temp = parent[tempKey];
        }
        reverse(path.begin(), path.end());

        if (stopAdjacent && path.size() > 1) {
            path.pop_back();
        }
    }

    return path;
}
