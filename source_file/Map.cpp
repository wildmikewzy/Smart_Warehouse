#include "Map.h"
#include <graphics.h>       // 引入 EasyX 图形库头文件，提供绘图功能

/**
 * @brief ：Map的构造函数：初始化地图，将所有格子设为空地 (0)
 */
Map::Map() {
    for (int i = 0; i < MAP_LENGTH; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            data[i][j] = 0; // 默认初始化为空地
        }
    }
}

/**
 * @brief 判断指定位置是否可以通行
 */
bool Map::isWalkable(float x, float y) const {
    // 首先进行边界检查，防止数组越界
    if (x < 0 || x >= MAP_LENGTH || y < 0 || y >= MAP_WIDTH) {
        return false;
    }
    // 只有空地 (0) 和 出货点 (2) 允许机器人进入
    return (data[static_cast<int>(x)][static_cast<int>(y)] == 0 || data[static_cast<int>(x)][static_cast<int>(y)] == 2);
}

/**
 * @brief 设置地图元素（0:空地, 1:货架, 2:出货点）
 */
void Map::setObstacle(float x, float y, int type) {
    if (x >= 0 && x < MAP_LENGTH && y >= 0 && y < MAP_WIDTH) {
		data[static_cast<int>(x)][static_cast<int>(y)] = type;      // 将指定位置设置为对应类型，由于输入参数是float类型，先转换为int以对应数组索引
    }
}

/**
 * @brief 核心绘图逻辑：将 20x20 的数据转化为视觉画面
 */
void Map::draw() const {
    for (int i = 0; i < MAP_LENGTH; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {

            // 计算当前格子的左上角像素坐标
            int left = i * GRID_SIZE;
            int top = j * GRID_SIZE;
            int right = (i + 1) * GRID_SIZE;
            int bottom = (j + 1) * GRID_SIZE;

            // 根据不同的逻辑类型填充颜色
            if (data[i][j] == 1) {
                // 1: 货架 -> 深灰色方块
                setfillcolor(RGB(100, 100, 100));
                fillrectangle(left + 2, top + 2, right - 2, bottom - 2);
            }
            else if (data[i][j] == 2) {
                // 2: 出货点/工作站 -> 浅绿色方块
                setfillcolor(RGB(144, 238, 144));
                solidrectangle(left + 1, top + 1, right - 1, bottom - 1);
            }
            // 0 是空地，由 GUI::drawGrid() 画出的背景处理
        }
    }
}