#include <graphics.h>
#include <cstring>
#include "Map.h"

Map::Map() {
    for (int i = 0; i < MAP_LENGTH; i++)
        for (int j = 0; j < MAP_WIDTH; j++)
            data[i][j] = 0;
}
/**
* @brief 判断格点是否可行走
*/
bool Map::isWalkable(int x, int y) const {
    if (x < 0 || x >= MAP_LENGTH || y < 0 || y >= MAP_WIDTH)
        return false;
    return (data[x][y] == 0 || data[x][y] == 2);
}
/**
* @brief 设置格点障碍物类型
*/
void Map::setObstacle(int x, int y, int type) {
    if (x >= 0 && x < MAP_LENGTH && y >= 0 && y < MAP_WIDTH)
        data[x][y] = type;
}
/**
* @brief 获取格点类型(0:空地, 1:货架, 2:出货点)
*/
int Map::getType(int x, int y) const {
    if (x < 0 || x >= MAP_LENGTH || y < 0 || y >= MAP_WIDTH) return -1;
    return data[x][y];
}

/** 
 *@brief 地图绘制函数
*/
void Map::draw() const {
    for (int i = 0; i < MAP_LENGTH; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            int left = i * GRID_SIZE;
            int top = j * GRID_SIZE;
            int right = (i + 1) * GRID_SIZE;
            int bottom = (j + 1) * GRID_SIZE;

            if (data[i][j] == 1) {
                setfillcolor(RGB(80, 80, 80));
                fillrectangle(left + 5, top + 5, right + 3, bottom + 3);
                setfillcolor(RGB(150, 150, 150));
                fillrectangle(left + 3, top + 3, right - 2, bottom - 2);
            }
            else if (data[i][j] == 2) {
                setfillcolor(RGB(100, 180, 100));
                solidroundrect(left + 6, top + 6, right - 2, bottom - 2, 8, 8);
                setfillcolor(RGB(144, 238, 144));
                solidroundrect(left + 2, top + 2, right - 6, bottom - 6, 8, 8);
                settextcolor(RED);
                settextstyle(14, 0, _T("Arial"));
                setbkmode(TRANSPARENT);
                outtextxy(left + 8, top + 10, _T("OUT"));
            }
        }
    }
}