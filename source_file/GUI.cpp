#include "GUI.h"
#include <string>

GUI::GUI(int l, int w) : screenLength(l), screenWideth(w) {
    initgraph(l, w, EX_SHOWCONSOLE);
    setbkcolor(WHITE);
}

GUI::~GUI() {
    closegraph();
}

void GUI::render(const WarehouseManager& manager) {
    cleardevice();
    drawGrid();

    // 画地图上的障碍物
    manager.getMap().draw();

    // 画机器人
    for (const auto& r : manager.getRobots()) {
        setfillcolor(r.id == 1 ? BLUE : RED);
        // 将网格坐标转为像素坐标并居中
        int px = r.currentPos.x * GRID_SIZE + GRID_SIZE / 2;
        int py = r.currentPos.y * GRID_SIZE + GRID_SIZE / 2;
        fillcircle(px, py, GRID_SIZE / 3);
    }

    drawStatusPanel(manager.getRobots());
}

void GUI::drawGrid() {
    setlinecolor(LIGHTGRAY);
    for (int i = 0; i <= MAP_LENGTH; i++)
        line(i * GRID_SIZE, 0, i * GRID_SIZE, MAP_WIDTH * GRID_SIZE);
    for (int i = 0; i <= MAP_WIDTH; i++)
        line(0, i * GRID_SIZE, MAP_LENGTH * GRID_SIZE, i * GRID_SIZE);
}

void GUI::drawStatusPanel(const std::vector<Robot>& robots) {
    settextcolor(BLACK);
    outtextxy(MAP_LENGTH * GRID_SIZE + 20, 20, "Smart_Warehouse 系统");
    // 可以在这里循环显示 robots 的状态...
}