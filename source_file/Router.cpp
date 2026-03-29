/*核心的算法接口*/
#include "Router.h"

std::vector<Point> Router::getPath(Point start, Point end, const Map& map) {        //输入起点和重点，并传入地图，函数返回路径点序列
    std::vector<Point> path;       //存放路径点向量

    // TODO:  算法工程师需要在这里实现 A* 算法

    // 目前先写一个“临时逻辑”：直接瞬移到终点，保证程序能跑通
    path.push_back(end);
    return path;
}