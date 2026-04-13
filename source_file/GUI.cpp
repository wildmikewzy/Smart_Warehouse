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
    settextstyle(20, 0, _T("微软雅黑"));

    int panelX = MAP_LENGTH * GRID_SIZE + 20;
    int yOffset = 20;

    outtextxy(panelX, yOffset, _T("Smart_Warehouse 系统"));
    yOffset += 30;

    settextstyle(16, 0, _T("宋体"));
    outtextxy(panelX, yOffset, _T("机器人状态："));
    yOffset += 25;

    for (const auto& r : robots) {
        // 将枚举状态转换为字符串
        std::string statusStr;
        switch (r.status) {
        case RobotStatus::IDLE:    statusStr = "空闲"; break;
        case RobotStatus::MOVING:  statusStr = "移动中"; break;
        case RobotStatus::LOADING: statusStr = "装载"; break;
        case RobotStatus::ERROR_:  statusStr = "错误"; break;
        default:                   statusStr = "未知"; break;
        }

        char info[100];
        sprintf_s(info, "ID: %d  位置: (%d, %d)  状态: %s",
            r.id, r.currentPos.x, r.currentPos.y, statusStr.c_str());

        // 根据机器人ID设置不同颜色
        settextcolor(r.id == 1 ? BLUE : RED);
        outtextxy(panelX, yOffset, _T(info));
        yOffset += 22;
    }
    settextcolor(BLACK); // 恢复默认
}

void GUI::handleMouseClick() {
    // 检测是否有鼠标左键按下
    if (MouseHit()) {
        MOUSEMSG msg = GetMouseMsg();
        if (msg.uMsg == WM_LBUTTONDOWN) {
            int mouseX = msg.x;
            int mouseY = msg.y;

            // 只处理地图区域内的点击（排除右侧面板）
            if (mouseX >= 0 && mouseX < MAP_LENGTH * GRID_SIZE &&
                mouseY >= 0 && mouseY < MAP_WIDTH * GRID_SIZE) {

                int gridX = mouseX / GRID_SIZE;
                int gridY = mouseY / GRID_SIZE;

                // 输出到控制台
                printf("[鼠标交互] 点击网格坐标: (%d, %d)\n", gridX, gridY);
            }
        }
    }
}