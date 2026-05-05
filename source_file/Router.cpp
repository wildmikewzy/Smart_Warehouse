/*核心的算法接口*/
#include "Router.h"
#include <vector>
#include<queue>
#include<map>
#include<cmath>
#include<algorithm>
#include <climits>
using namespace std;

//内部辅助的结构体，用来优先队列排序
struct Node {
    Point pt;
  
    //C++优先排列大顶堆，我们算路径要小顶堆，重载大于运算符来定义比较Node
    //bool operator > (const Node& other)const {
       // return dist > other.dist;
    //}

    //第二周更新
    int g;//起点代价
    int h;//启发值
    int f()const { return g + h; }
    bool operator>(const Node& other)const {
        return f() > other.f();
    }
};
//第二周更新
    //1.更新搜索策略
    //2、更新移动方式，避免锯齿化
    //3.更新对应路线的记录

    //新增辅助函数，启发函数,计算距离来实现8方向的移动策略
int heuristic(const Point& a, const Point& b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}
// 视线检查（Bresenham 直线算法，仅检查水平/垂直直线，不产生对角线跃迁）
// 因为机器人只支持四方向移动，所以只允许 dx==0 或 dy==0 的直线可达。
static bool hasLineOfSight(const Point& a, const Point& b, const Map& map) {
    int x0 = static_cast<int>(round(a.x));
    int y0 = static_cast<int>(round(a.y));
    int x1 = static_cast<int>(round(b.x));
    int y1 = static_cast<int>(round(b.y));

    // 只处理水平或垂直直线，对角线直接不可见（防止出现斜线路径）
    if (x0 != x1 && y0 != y1)
        return false;

    // 边界辅助函数（避免越界访问）
    auto isWithinMap = [](int x, int y) -> bool {
        return x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_LENGTH;
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
vector<Point> Router::getPath(Point start, Point end, const Map& map) {        //输入起点和重点，并传入地图，函数返回路径点序列
    vector<Point> path;       //存放路径点向量

    // TODO:  算法工程师需要在这里实现A*算法，根据地图数据计算出从起点到重点的路径，并将路径点以此加入到path向量中。
    // 辅助：检查点是否在地图范围内且可行走
    auto isPointValid = [&](const Point& p) -> bool {
        int ix = static_cast<int>(round(p.x));
        int iy = static_cast<int>(round(p.y));
        if (ix < 0 || ix >= MAP_WIDTH || iy < 0 || iy >= MAP_LENGTH)
            return false;
        return map.isWalkable(p.x, p.y);
        };
    //1.特殊情况：起点或终点不可行走，直接返回路径为空
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
    //2.初始化数据结构 distMap记录最短距离，parentMap父节点记录来回溯行走路径
    //dist距离表（第二周修改后变为g值表）
    std::vector<std::vector<int>>g(MAP_LENGTH, std::vector<int>(MAP_WIDTH, INT_MAX));
    //parent路径表
    vector<vector<Point>>parent(MAP_LENGTH, vector<Point>(MAP_WIDTH, { -1,-1 }));
    //pq优先的队列（第二周后按f值的最小堆）
    priority_queue<Node, vector<Node>, greater<Node>>pq;
    //起点初始化
    g[static_cast<int>(start.x)][static_cast<int>(start.y)] = 0;
    //第二周更新，g=0.h=启发值
    int h_Start = heuristic(start, end);
    pq.push({ start,0,h_Start });

    

    //方向向量：上下左右
    int dx[] = { 0,0,1,-1 };
    int dy[] = { 1,-1,0,0 };
    //定义判断找到路径的工具
    bool found = false;

    //3.开始搜索
    while (!pq.empty()) {
        //1.取出队列中距离起点最近的点
        Node current = pq.top();//队首
        pq.pop();//删除处理完的节点
        //把节点的坐标单独拿出来
        Point currPt = current.pt;

        //2.终止条件：当前点即为终点
        if (currPt == end) {
            found = true;
            break;
        }

        //3.跳过无效数据
        if (current.g > g[static_cast<int>(currPt.x)][static_cast<int>(currPt.y)]) continue;

        //4.遍历当前点的四个方向看看还有没有更短路径
        for (int i = 0; i < 4; i++) {
			float px = (currPt.x) + dx[i];// 计算新点的坐标
			float py = (currPt.y) + dy[i];// 计算新点的坐标

            // 边界检查
            if (px < 0 || px >= MAP_WIDTH || py < 0 || py >= MAP_LENGTH)
                continue;

            //5.判断能不能走
            if (map.isWalkable(px, py)) {
                //能走，一步路径加一
                int newG = g[static_cast<int>(currPt.x)][static_cast<int>(currPt.y)] + 1;

                //6.如果路径更短，更新
                if (newG < g[static_cast<int>(px)][static_cast<int>(py)]) {
                    g[static_cast<int>(px)][static_cast<int>(py)] = newG;//更新最短路径
                    parent[static_cast<int>(px)][static_cast<int>(py)] = currPt;//记录路径
                   //
                    int h_Neighbor = heuristic({ px,py }, end);
                    //
                    pq.push({ { px,py }, newG, h_Neighbor });//新节点加入优先队列，接着搜索比较
                }
            }
        }
    }

    //4.回溯路径
    if (found) {//1.成功找到终点，进行路径回溯
        //找到终点
        Point temp = end;
        //循环倒着走，到起点
        while (!(temp == Point{ -1,-1 })) {
            path.push_back(temp);//当前点存入路径
            temp = parent[static_cast<int>(temp.x)][static_cast<int>(temp.y)];//当前点变成上一个点再次循环
        }
        //反转倒着循环的路径，正向显示运动路径
        reverse(path.begin(), path.end());
    }
    //只有当有路可走的时候才把终点加入路径，否则返回空路径
    if (!path.empty()) {
        path = smoothPath(path, map);
    }
    return path;
}