#pragma once
#include <vector>

/**
* @brief 定义一个点结构体，表示地图上的位置
*/
struct Point {
    int x, y;
    // 重载判，方便算法判断是否到达目标
    bool operator == (const Point& other) const { return x == other.x && y == other.y; }
};

enum class RobotStatus { IDLE, MOVING, LOADING, ERROR_};        //定义一个机器人状态枚举体，分别表示空闲、移动、装载和错误状态

// === 坐标系协议 ===
// X轴：对应 MAP_LENGTH (水平方向)
// Y轴：对应 MAP_WIDTH  (垂直方向)
const int GRID_SIZE = 35;           //每个网格的像素大小
const int MAP_LENGTH = 20;           //地图长度，单位为网格数 x
const int MAP_WIDTH = 20;          //地图宽度，单位为网格数  y

// 侧边栏宽度
const int UI_PANEL_WIDTH = 250;

// 窗口总尺寸
const int WIN_WIDTH = MAP_LENGTH * GRID_SIZE + UI_PANEL_WIDTH;
const int WIN_HEIGHT = MAP_WIDTH * GRID_SIZE;