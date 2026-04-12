/*核心的算法接口*/
#include "Router.h"
#include <vector>
#include<queue>
#include<map>
#include<algorithm>
using namespace std;

//内部辅助的结构体，用来优先队列排序
struct Node {
    Point pt;
    //刻画从起点到当前点的量度代价g
    int dist;
    //C++优先排列大顶堆，我们算路径要小顶堆，重载大于运算符来定义比较Node
    bool operator>(const Node& other)const {
        return dist > other.dist;
    }
};

//实现Dijkstra算法
vector<Point> Router::getPath(Point start, Point end, const Map& map) {        //输入起点和重点，并传入地图，函数返回路径点序列
    vector<Point> path;       //存放路径点向量

    // TODO:  算法工程师需要在这里实现Dijkstra算法，根据地图数据计算出从起点到重点的路径，并将路径点以此加入到path向量中。

    //1.特殊情况：起点或终点不可行走，直接返回路径为空
    if (!map.isWalkable(start.x, start.y) || !map.isWalkable(end.x, end.y)) {
        return path;
    }

    //2.初始化数据结构 distMap记录最短距离，parentMap父节点记录来回溯行走路径
    //dist距离表
    std::vector<std::vector<int>>dist(MAP_LENGTH, std::vector<int>(MAP_WIDTH, INT_MAX));
    //parent路径表
    std::vector<std::vector<Point>>parent(MAP_LENGTH, std::vector<Point>(MAP_WIDTH, { -1,-1 }));
    //pq优先的队列
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>>pq;

    //起点初始化
    dist[start.x][start.y] = 0;
    pq.push({ start,0 });

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
        if (current.dist > dist[currPt.x][currPt.y]) continue;

        //4.遍历当前点的四个方向看看还有没有更短路径
        for (int i = 0; i < 4; i++) {
            int px = currPt.x + dx[i];//那个方向的x坐标
            int py = currPt.y + dy[i];//那个方向的y坐标

            //5.判断能不能走
            if (map.isWalkable(px, py)) {
                //能走，一步路径加一
                int newDist = dist[currPt.x][currPt.y] + 1;

                //6.如果路径更短，更新
                if (newDist < dist[px][py]) {
                    dist[px][py] = newDist;//更新最短路径
                    parent[px][py] = currPt;//记录路径
                    pq.push({ {px,py},newDist });//新节点加入优先队列，接着搜索比较
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
            temp = parent[temp.x][temp.y];//当前点变成上一个点再次循环
        }
        //反转倒着循环的路径，正向显示运动路径
        std::reverse(path.begin(), path.end());
        //起点不需要再走
        if (!path.empty())path.erase(path.begin());

    }
    path.push_back(end);        //直接把终点加入到路径里面，模拟瞬间移动到重点的效果。
    return path;
}