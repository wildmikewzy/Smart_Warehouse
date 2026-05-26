/*核心的算法接口*/
#include "Router.h"
#include <vector>
#include<queue>
#include<map>
#include<cmath>
#include<algorithm>
#include <climits>
#include "Robot.h"
using namespace std;

//内部辅助的结构体，用来优先队列排序
struct Node {
    Point pt;

    int g;//起点代价
    int h;//启发值
    int f()const { return g + h; }

	bool operator>(const Node& other)const {        //优先队列默认是最大堆，我们需要最小堆从小到达排序，所以重载 > 运算符 
        return f() > other.f();
    }
};

/**
* @brief 曼哈顿距离启发函数，适用于四方向移动的网格地图
*/
int heuristic(const Point& a, const Point& b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}
// 视线检查（Bresenham 直线算法，仅检查水平/垂直直线，不产生对角线跃迁）
// 因为机器人只支持四方向移动，所以只允许 dx==0 或 dy==0 的直线可达。
static bool hasLineOfSight(const Point& a, const Point& b, const Map& map) {
    int x0 = a.x;
    int y0 = a.y;
    int x1 = b.x;
    int y1 = b.y;

    // 只处理水平或垂直直线，对角线直接不可见（防止出现斜线路径）
    if (x0 != x1 && y0 != y1)
        return false;

    // 边界辅助函数（避免越界访问）
    auto isWithinMap = [](int x, int y) -> bool {
        return x >= 0 && x < MAP_LENGTH && y >= 0 && y < MAP_WIDTH;
    };

    // 如果起点或终点超出地图范围，不可见
    if (!isWithinMap(x0, y0) || !isWithinMap(x1, y1))
        return false;

    // Bresenham 直线算法（仅沿轴线方向移动）
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0, y = y0;
    while (true) {
        // 检查当前格子是否可行走（包括起点和终点）
        if (!map.isWalkable(x, y))
            return false;

        if (x == x1 && y == y1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    return true;
}

//路径平滑处理，让路的拐点更少
vector<Point>smoothPath(const vector<Point>& raw, const Map& map) {
    vector<Point>result;
    if (raw.empty())return result;
    //保留起点
    result.push_back(raw.front());
    size_t i = 0;
    while (i < raw.size() - 1) {
        size_t j = raw.size() - 1;
        //当前点i往后看，找最远的可见点j，跳过中间点
        while (j > i + 1) {
            if (hasLineOfSight(raw[i], raw[j], map)) {
                result.push_back(raw[j]);
                i = j;
                break;
            }
            --j;
        }
        //否则就一步一步走
        if (j == i + 1) {
            ++i;
            result.push_back(raw[i]);
        }
    }
    return result;
}


//实现A*算法
vector<Point> Router::getPath(Point start, Point end, const Map& map, bool stopAdjacent) {
    vector<Point> path;

    // 辅助：检查点是否在地图范围内且可行走 (去除 round 转换)
    auto isPointValid = [&](const Point& p) -> bool {
        if (p.x < 0 || p.x >= MAP_LENGTH || p.y < 0 || p.y >= MAP_WIDTH)
            return false;
        return map.isWalkable(p.x, p.y);
    };

    // 1.特殊情况：起点或终点不可行走，直接返回路径为空
    if (!map.isWalkable(start.x, start.y) || !map.isWalkable(end.x, end.y)) {
        return path;
    }
    if (!isPointValid(start) || !isPointValid(end))
        return path;

    // 起点等于终点
    if (start.x == end.x && start.y == end.y) {
        path.push_back(start);
        return path;
    }

    // 初始化数据结构
    std::vector<std::vector<int>> g(MAP_LENGTH, std::vector<int>(MAP_WIDTH, INT_MAX));
    vector<vector<Point>> parent(MAP_LENGTH, vector<Point>(MAP_WIDTH, { -1,-1 }));
    priority_queue<Node, vector<Node>, greater<Node>> pq;

    // 起点初始化 (去除 static_cast)
    g[start.x][start.y] = 0;
    int h_Start = heuristic(start, end);
    pq.push({ start, 0, h_Start });

    int dx[] = { 0,0,1,-1 };
    int dy[] = { 1,-1,0,0 };
    bool found = false;

    // 3.开始搜索
// 3. 开始搜索
    while (!pq.empty()) {
        Node current = pq.top();
        pq.pop();
        Point currPt = current.pt;

        if (currPt == end) {
            found = true;
            break;
        }

        int cx = currPt.x;
        int cy = currPt.y;
        if (current.g > g[cx][cy]) continue;

        // 4. 遍历当前点的四个方向
        for (int i = 0; i < 4; i++) {
            int px = currPt.x + dx[i];
            int py = currPt.y + dy[i];

            // 边界与障碍物检查
            if (px < 0 || px >= MAP_LENGTH || py < 0 || py >= MAP_WIDTH) continue;
            if (!map.isWalkable(px, py)) continue;

            // 【核心修改：计算转弯惩罚】
            int turnPenalty = 0;

            // 如果当前点有父节点，说明有“前驱方向”
            if (parent[currPt.x][currPt.y].x != -1) {
                Point lastPt = parent[currPt.x][currPt.y];

                // 算出上一格的移动方向
                int lastDx = currPt.x - lastPt.x;
                int lastDy = currPt.y - lastPt.y;

                // 算出这一格的移动方向
                int currentDx = px - currPt.x;
                int currentDy = py - currPt.y;

                // 如果方向不一致，说明拐弯了，狠狠惩罚它！
                if (lastDx != currentDx || lastDy != currentDy) {
                    turnPenalty = 5; // 基础移动代价是1，转弯惩罚给5，意味着宁愿多绕4格直道也不想转弯
                }
            }

            // 5. 加上转弯惩罚计算新 G 值
            int newG = g[currPt.x][currPt.y] + 1 + turnPenalty;

            // 6. 如果路径更短（算上转弯成本后），更新
            if (newG < g[px][py]) {
                g[px][py] = newG;
                parent[px][py] = currPt;
                int h_Neighbor = heuristic({ px, py }, end);
                pq.push({ { px, py }, newG, h_Neighbor });
            }
        }
    }
    // 4.回溯路径
    if (found) {
        Point temp = end;
        while (!(temp == Point{ -1,-1 })) {
            path.push_back(temp);
            temp = parent[temp.x][temp.y]; // 去除静态转换
        }
        reverse(path.begin(), path.end());

        // 要求停在相邻点（即目标是货架）并且路径有得可退
        if (stopAdjacent && path.size() > 1) {
            path.pop_back();
        }
    }

    // 平滑处理
    if (!path.empty()) {
        path = smoothPath(path, map);
    }
    return path;
}