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

/** *@brief 地图绘制函数（ABC热度分类可视化版）
*/
void Map::draw() const {
    for (int i = 0; i < MAP_LENGTH; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            int left = i * GRID_SIZE;
            int top = j * GRID_SIZE;
            int right = (i + 1) * GRID_SIZE;
            int bottom = (j + 1) * GRID_SIZE;

            // 1. 如果当前格点是货架
            if (data[i][j] == 1) {
                // 🌟【核心逻辑】：像素级同步你的WMS曼哈顿热度分配算法
                int dist = abs(i - 19) + abs(j - 10);

                // 定义货架的表层颜色（Main）和底层 3D 浮雕阴影颜色（Shadow）
                COLORREF colorShadow = RGB(80, 80, 80);
                COLORREF colorMain = RGB(150, 150, 150);

                if (dist <= 12) {
                    // 🔥 A类货架：温暖的浅橙色（高频区）
                    colorShadow = RGB(210, 120, 40);
                    colorMain = RGB(255, 185, 110);
                }
                else if (dist <= 20) {
                    // 🧊 B类货架：温和的浅蓝色（过渡区）
                    colorShadow = RGB(85, 140, 185);
                    colorMain = RGB(150, 206, 245);
                }
                else {
                    // ⛰️ C类货架：静谧的浅灰青色（低频区）
                    colorShadow = RGB(140, 145, 150);
                    colorMain = RGB(205, 212, 215);
                }

                // 渲染 3D 浮雕阴影底框
                setlinecolor(colorShadow);
                setfillcolor(colorShadow);
                fillrectangle(left + 5, top + 5, right + 3, bottom + 3);

                // 渲染 货架实体表面
                setlinecolor(colorMain);
                setfillcolor(colorMain);
                fillrectangle(left + 3, top + 3, right - 2, bottom - 2);
            }
            // 2. 如果当前格点是出货点
            else if (data[i][j] == 2) {
                // 出货点保持原来的经典高亮绿底与红色“OUT”字样
                setlinecolor(RGB(100, 180, 100));
                setfillcolor(RGB(100, 180, 100));
                solidroundrect(left + 6, top + 6, right - 2, bottom - 2, 8, 8);

                setlinecolor(RGB(144, 238, 144));
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