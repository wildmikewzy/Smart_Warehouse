/*路径规划*/
#pragma once
#include "common.h"
#include "Map.h"

class Router {
public:
    static std::vector<Point> getPath(Point start, Point end, const Map& map, bool stopAdjacent = false);       //算法工程师骆晗旭负责这个函数的编写
};