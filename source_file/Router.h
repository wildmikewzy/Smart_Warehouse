/*路径规划*/
#pragma once
#include "common.h"
#include "Map.h"
#include <vector>
#include <set>
#include <map>
#include <string>
// 1. 定义时空点
struct TimePoint {
    int x;
    int y;
    int t; // 时间戳（网格步数/帧数）

    // 用于 std::set 或 std::map 的比较运算符
    bool operator<(const TimePoint& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return t < other.t;
    }
    bool operator==(const TimePoint& other) const {
        return x == other.x && y == other.y && t == other.t;
    }
};

// 2. 定义全局/区域预定表结构
struct ReservationTable {
    // 记录某一时刻某个格子被哪个机器人占领
    // key: {x, y, t}, value: robotId
    std::map<TimePoint, int> vertexReservations;

    // 记录边占领，防止两车“对头穿过”（A从(4,9)->(4,8)，B从(4,8)->(4,9)同时发生）
    // key: 字符串 "x1,y1->x2,y2@t", value: robotId
    std::set<std::string> edgeReservations;
};

class Router {
public:
    /**
     * @brief 时空A*路径规划接口
     * @param start 起点
     * @param end 最终终点
     * @param map 基础静态地图
     * @param table 全局预定表指针
     * @param robotId 当前寻路机器人的ID
     * @param startTime 当前寻路的初始时间戳（默认从0开始，如果是半路重寻路则是当前时间）
     * @param stopAdjacent 是否停在相邻格
     */
    static std::vector<Point> getPath(Point start, Point end, const Map& map,
        const ReservationTable& table, int robotId,
        int startTime = 0, bool stopAdjacent = false);
};