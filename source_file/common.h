#pragma once
#include <vector>
#include <cmath>

/**
* @brief 定义一个点结构体，表示地图上的位置
*/
struct Point {
    int x, y;
    // 判等重载恢复为整数绝对相等
    bool operator == (const Point& other) const { 
        return x == other.x && y == other.y;
    }
};

enum class RobotStatus { IDLE, PREPARING, MOVING, LOADING, ERROR_};

// === 坐标系协议 ===
const int GRID_SIZE = 35;           //每个网格的像素大小
const int MAP_LENGTH = 20;           //地图长度
const int MAP_WIDTH = 20;          //地图宽度

// 侧边栏宽度
const int UI_PANEL_WIDTH = 350;

// 窗口总尺寸
const int WIN_WIDTH = MAP_LENGTH * GRID_SIZE + UI_PANEL_WIDTH;
const int WIN_HEIGHT = MAP_WIDTH * GRID_SIZE;