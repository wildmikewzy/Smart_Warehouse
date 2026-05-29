/*核心的算法接口*/
#include "Router.h"
#include <vector>
#include <queue>
#include <map>
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

// 辅助函数：生成边占领的唯一Key
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

// 移除原先的 smoothPath。
// 原因：在引入时间维度的预定表系统后，严禁使用盲目的射线平滑！
// 射线平滑会跳过中间点，导致小车物理上直接穿过中间时刻别人预定的格子。时空路径本身就是像素级连续的。

vector<Point> Router::getPath(Point start, Point end, const Map& map,
    const ReservationTable& table, int robotId,
    int startTime, bool stopAdjacent) {
    vector<Point> path;

    // 1. 基础合法性检查
    if (!map.isWalkable(start.x, start.y) || !map.isWalkable(end.x, end.y)) return path;

    // 起点等于终点且无需等待
    if (start.x == end.x && start.y == end.y) {
        path.push_back(start);
        return path;
    }

    // 因为状态空间变好了 (x, y, t)，我们使用 map 来存储三维的 G 值和父节点
    // key 字符串格式: "x,y,t"
    auto makeKey = [](int x, int y, int t) {
        return to_string(x) + "," + to_string(y) + "," + to_string(t);
        };

    std::map<string, int> g;
    std::map<string, STNode> parent;
    priority_queue<STNode, vector<STNode>, greater<STNode>> pq;

    // 初始化起点
    string startKey = makeKey(start.x, start.y, startTime);
    g[startKey] = 0;
    int h_Start = heuristic(start, end);
    pq.push({ start, startTime, 0, h_Start });

    // 5个行动方向：四个轴向移动 + 1个原地等待
    int dx[] = { 0, 0, 1, -1, 0 };
    int dy[] = { 1, -1, 0, 0, 0 };

    bool found = false;
    STNode endNode;

    // 设置最大时空搜索窗口，防止死锁时搜索无限扩散（100步以内必须解决冲突）
    int maxTimeWindow = startTime + 100;

    while (!pq.empty()) {
        STNode current = pq.top();
        pq.pop();

        // 终点判定
        if (current.pt == end) {
            // 如果到了终点，我们通常还需要确保该机器人在到达终点后不会立刻被后面开过来的车碾过去
            // 可以在此处多验证几步之后的终点安全性
            found = true;
            endNode = current;
            break;
        }

        if (current.t >= maxTimeWindow) continue; // 超出时间窗上限

        string currKey = makeKey(current.pt.x, current.pt.y, current.t);
        if (current.g > g[currKey]) continue;

        // 遍历 4 个方向移动 + 原地等待
        for (int i = 0; i < 5; i++) {
            int px = current.pt.x + dx[i];
            int py = current.pt.y + dy[i];
            int nextT = current.t + 1; // 时间步推进 1

            // 边界与静态障碍检查
            if (px < 0 || px >= MAP_LENGTH || py < 0 || py >= MAP_WIDTH) continue;
            if (!map.isWalkable(px, py)) continue;

            // 反向边冲突：防止两车对头交换格子
            std::string reverseEdgeKey = to_string(px) + "," + to_string(py) + "->" +
                to_string(current.pt.x) + "," + to_string(current.pt.y) + "@" + to_string(nextT);
            if (table.edgeReservations.count(reverseEdgeKey) > 0) {
                continue;
            }

            // 时空安全检查（点冲突等）
            if (!isTimeSpaceSafe(current.pt.x, current.pt.y, px, py, nextT, robotId, table)) {
                continue; // 当前邻居被占用或发生对头冲突
            }

            // 计算边代价与转向罚分
            int edgeCost = 1;
            int turnPenalty = 0;

            if (i == 4) {
                // 原地等待：略高代价，避免无意义等待
                edgeCost = 2;
            }
            else {
                // 转向罚分
                string currParentKey = makeKey(current.pt.x, current.pt.y, current.t);
                if (parent.count(currParentKey) > 0) {
                    Point lastPt = parent[currParentKey].pt;
                    int lastDx = current.pt.x - lastPt.x;
                    int lastDy = current.pt.y - lastPt.y;
                    int currentDx = px - current.pt.x;
                    int currentDy = py - current.pt.y;

                    if ((lastDx != currentDx || lastDy != currentDy) && (lastDx != 0 || lastDy != 0)) {
                        turnPenalty = 5;
                    }
                }
            }

            int newG = current.g + edgeCost + turnPenalty;
            string nextKey = makeKey(px, py, nextT);

            if (g.count(nextKey) == 0 || newG < g[nextKey]) {
                g[nextKey] = newG;
                parent[nextKey] = current;
                int h_Neighbor = heuristic({ px, py }, end);
                pq.push({ {px, py}, nextT, newG, h_Neighbor });
            }
        }
    }

    // 4. 回溯时空路径
    if (found) {
        STNode temp = endNode;
        while (true) {
            path.push_back(temp.pt);
            string tempKey = makeKey(temp.pt.x, temp.pt.y, temp.t);
            if (parent.count(tempKey) == 0) break;
            temp = parent[tempKey];
        }
        reverse(path.begin(), path.end());

        if (stopAdjacent && path.size() > 1) {
            path.pop_back();
        }
    }

    return path;
}
