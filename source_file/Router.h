/*路径规划*/
#pragma once
#include "common.h"
#include "Map.h"

class Router {
public:
    // 核心接口：输入起点终点，返回路径点序列
    
    // 角色 B（算法工程师） 只要在这个函数里折腾 A* 算法即可
    static std::vector<Point> getPath(Point start, Point end, const Map& map);
};