#pragma once
#pragma once

#include <vector>
#include <cmath>
#include <string>

struct Point {
    int x, y;
    bool operator == (const Point& other) const {
        return x == other.x && y == other.y;
    }
	Point(int _x = 0, int _y = 0) : x(_x), y(_y) {}
};

// 升级版：工业 AGV 业务全生命周期状态机
enum class RobotStatus {
    IDLE,               // 在左侧待命区充能/闲置
    MOVING_TO_PICK,     // 正在前往货架取货点的路上
    LOADING,            // 到达取货点，正在装货锁定中
    MOVING_TO_DELIVER,  // 装货完毕，正携货前往出货口
    UNLOADING,          // 到达出货口，正在卸货锁定中
    RETURNING_BUFFER,       //出货完毕，前往返回缓冲区
    ERROR_      //机器人错误状态
};

enum class SKUType { A, B, C };

// 拓扑业务站点结构体
struct Station {
    int stationId;
    Point shelfPoint;   // 实体货架网格坐标 (障碍物)
    Point dockPoint;    // 小车取货时的靠泊通行网格坐标 (通道)
    SKUType sku;        // 该货架存放的货物类型
};

// === 坐标系协议 ===
const int GRID_SIZE = 35;
const int MAP_LENGTH = 20;
const int MAP_WIDTH = 20;

const int UI_PANEL_WIDTH = 350;
const int WIN_WIDTH = MAP_LENGTH * GRID_SIZE + UI_PANEL_WIDTH;
const int WIN_HEIGHT = MAP_WIDTH * GRID_SIZE;