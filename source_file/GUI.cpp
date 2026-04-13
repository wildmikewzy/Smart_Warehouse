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
    cleardevice();      //清屏函数
    drawGrid();         //画网格线

    // 画地图上的障碍物
    manager.getMap().draw();

    // 画机器人
    for (const auto& r : manager.getRobots()) {
        // --- 绘制机器人的轨迹线 ---
        if (r.historyPath.size() > 1) {
            setlinecolor(r.id == 1 ? LIGHTBLUE : LIGHTRED); // 根据机器人 ID 区分轨迹颜色
            setlinestyle(PS_DOT, 2); // 使用虚线表示轨迹

            for (size_t i = 0; i < r.historyPath.size() - 1; i++) {
                // 将网格坐标转换为像素中心点坐标
                int x1 = r.historyPath[i].x * GRID_SIZE + GRID_SIZE / 2;
                int y1 = r.historyPath[i].y * GRID_SIZE + GRID_SIZE / 2;
                int x2 = r.historyPath[i + 1].x * GRID_SIZE + GRID_SIZE / 2;
                int y2 = r.historyPath[i + 1].y * GRID_SIZE + GRID_SIZE / 2;
                line(x1, y1, x2, y2);
            }
        }
        setfillcolor(r.id == 1 ? BLUE : RED);
        // 将网格坐标转为像素坐标并居中
        int px = r.currentPos.x * GRID_SIZE + GRID_SIZE / 2;
        int py = r.currentPos.y * GRID_SIZE + GRID_SIZE / 2;
        fillcircle(px, py, GRID_SIZE / 3);
    }

    drawStatusPanel(manager.getRobots());           //画出状态面板，显示每个机器人对应的状态（空闲、移动、装在、错误...）
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