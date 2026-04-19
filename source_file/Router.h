/*路径规划*/
#pragma once
#include "common.h"
#include "Map.h"

class Router {
public:
    
    // 算法工程师：骆晗旭 ，任务：在这个函数里折腾 Dijkstra 算法即可
    static std::vector<Point> getPath(Point start, Point end, const Map& map);
};