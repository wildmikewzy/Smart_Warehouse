#include "GUI.h"
#include <cmath>
#include <iostream>
#include <windows.h> // Windows API 头文件

GUI::GUI(int l, int w) : screenLength(l), screenWideth(w) {
    initgraph(l, w, EX_SHOWCONSOLE);
    setbkcolor(WHITE);
    startTime = std::chrono::steady_clock::now();
    lastTick = GetTickCount();
    lastBlinkTime = GetTickCount();

    dispatchButtonText = "货架业务派送";
    gridDispatchButtonText = "普通格点派送";
}

GUI::~GUI() {
    closegraph();
}

void GUI::render(const WarehouseManager& manager) {
    cleardevice();
    drawGrid();
    manager.getMap().draw();

    for (const auto& r : manager.getRobots()) {
        setfillcolor(r.id % 2 == 0 ? BLUE : RED);

        // 使用 r.realX / r.realY 绘制更平滑的实际位置
        int px = static_cast<int>(r.realX * GRID_SIZE) + GRID_SIZE / 2;
        int py = static_cast<int>(r.realY * GRID_SIZE) + GRID_SIZE / 2;
        fillcircle(px, py, GRID_SIZE / 3);

        char idStr[10];
        sprintf_s(idStr, "%d", r.id);
        settextcolor(WHITE);
        setbkmode(TRANSPARENT);
        settextstyle(16, 0, _T("微软雅黑"));
        outtextxy(px - 5, py - 8, _T(idStr));
    }

    bool drawRobotRing = false;
    bool drawRackRing = hasSelectedRack && showDispatchButton;
    bool drawTargetRing = hasSelectedTarget && showGridDispatchButton;
    Point robotRingPos = { -1, -1 };

    if (selectedRobotId != -1) {
        const auto& robots = manager.getRobots();
        for (const auto& r : robots) {
            if (r.id == selectedRobotId) {
                if (r.status == RobotStatus::IDLE) {
                    drawRobotRing = true;
                    robotRingPos = r.currentPos;
                }
                break;
            }
        }
    }

    bool shouldBlink = drawRobotRing || drawRackRing || drawTargetRing;
    if (shouldBlink) {
        updateBlink();
    }
    else {
        blinkVisible = true;
        lastBlinkTime = GetTickCount();
    }

    if (drawRobotRing) {
        drawSelectionRing(robotRingPos);
    }
    if (drawRackRing) {
        drawRackSelectionRing(selectedRackPos);
    }
    if (drawTargetRing) {
        drawTargetSelectionRing(selectedTargetPos);
    }

    drawStatusPanel(manager.getRobots(), hasSelectedRack, selectedRackPos, hasSelectedTarget, selectedTargetPos);

    // 右侧按钮
    if (showDispatchButton) {
        drawDispatchButton();
    }
    if (showGridDispatchButton) {
        drawGridDispatchButton();
    }

    DWORD currentTick = GetTickCount();
    DWORD delta = static_cast<DWORD>(currentTick - lastTick);
    lastTick = currentTick;
    updatePopups(delta);
    drawPopups();
}

void GUI::handleMouseClick(WarehouseManager& manager) {
    ExMessage msg;
    bool processedClick = false;

    while (peekmessage(&msg, EX_MOUSE)) {
        if (msg.message != WM_LBUTTONDOWN) {
            continue;
        }

        if (processedClick) {
            continue;
        }
        processedClick = true;

        int mx = msg.x;
        int my = msg.y;
        bool clickHandled = false;

        // 1. 点击了“货架业务派送”按钮
        if (showDispatchButton && mx >= dispatchButtonRect.left && mx <= dispatchButtonRect.right &&
            my >= dispatchButtonRect.top && my <= dispatchButtonRect.bottom)
        {
            if (selectedRobotId != -1 && hasSelectedRack) {
                // 【O(1) 极速查表】直接通过缓存获取站点ID，没有任何循环耗时
                int targetStationId = manager.getStationIdByGrid(selectedRackPos.x, selectedRackPos.y);

                if (targetStationId != -1) {
                    manager.dispatchRobot(selectedRobotId, targetStationId);
                    char buf[100];
                    sprintf_s(buf, "成功指派机器人 %d 前往货架站点 %d", selectedRobotId, targetStationId);
                    addPopup(buf);
                }
                else {
                    addPopup("异常：该货架未绑定有效的靠泊站点！", 3.0f);
                }

                hasSelectedRack = false;
                showDispatchButton = false;
            }
            clickHandled = true;
        }

        if (clickHandled) {
            continue;
        }

        // 2. 点击了“普通格点派送”按钮
        if (showGridDispatchButton && mx >= gridDispatchButtonRect.left && mx <= gridDispatchButtonRect.right &&
            my >= gridDispatchButtonRect.top && my <= gridDispatchButtonRect.bottom)
        {
            if (selectedRobotId != -1 && hasSelectedTarget) {
                // 【O(1) 无极查表】
                int targetStationId = manager.getStationIdByGrid(selectedTargetPos.x, selectedTargetPos.y);

                if (targetStationId != -1) {
                    manager.dispatchRobot(selectedRobotId, targetStationId);
                    char buf[100];
                    sprintf_s(buf, "机器人 %d 前往目标靠泊点站点 %d", selectedRobotId, targetStationId);
                    addPopup(buf);
                }
                else {
                    addPopup("点击位置未绑定拓扑网靠泊站点！", 3.0f);
                }

                hasSelectedTarget = false;
                showGridDispatchButton = false;
            }
            clickHandled = true;
        }

        if (clickHandled) {
            continue;
        }

        // 3. 点击地图网格区域
        int gridX = mx / GRID_SIZE;
        int gridY = my / GRID_SIZE;

        if (gridX >= 0 && gridX < MAP_LENGTH && gridY >= 0 && gridY < MAP_WIDTH) {

            // A. 检测是否点中了机器人
            bool clickedRobot = false;
            for (const auto& r : manager.getRobots()) {
                if (static_cast<int>(r.realX + 0.5f) == gridX && static_cast<int>(r.realY + 0.5f) == gridY) {
                    selectedRobotId = r.id;
                    clickedRobot = true;

                    hasSelectedRack = false;
                    showDispatchButton = false;
                    hasSelectedTarget = false;
                    showGridDispatchButton = false;

                    char buf[50];
                    sprintf_s(buf, "已选中机器人 %d", selectedRobotId);
                    addPopup(buf);
                    break;
                }
            }

            // B. 如果已经选了车，点的是普通网格
            if (!clickedRobot && selectedRobotId != -1) {
                const Map& warehouseMap = manager.getMap();

                if (!warehouseMap.isWalkable(gridX, gridY)) {
                    selectedRackPos = { gridX, gridY };
                    hasSelectedRack = true;
                    showDispatchButton = true;

                    hasSelectedTarget = false;
                    showGridDispatchButton = false;
                }
                else {
                    selectedTargetPos = { gridX, gridY };
                    hasSelectedTarget = true;
                    showGridDispatchButton = true;

                    hasSelectedRack = false;
                    showDispatchButton = false;
                }
            }
        }
    }
}
void GUI::drawGrid() {
    setlinecolor(LIGHTGRAY);
    for (int i = 0; i <= MAP_LENGTH; i++)
        line(i * GRID_SIZE, 0, i * GRID_SIZE, MAP_WIDTH * GRID_SIZE);
    for (int i = 0; i <= MAP_WIDTH; i++)
        line(0, i * GRID_SIZE, MAP_LENGTH * GRID_SIZE, i * GRID_SIZE);
}

void GUI::drawStatusPanel(const std::vector<Robot>& robots, bool hasRack, Point rackPos, bool hasTarget, Point targetPos) {
    settextcolor(BLACK);
    settextstyle(20, 0, _T("微软雅黑"));

    int panelX = MAP_LENGTH * GRID_SIZE + 20;
    int yOffset = 20;

    outtextxy(panelX, yOffset, _T("Smart_Warehouse 系统"));
    yOffset += 30;

    settextstyle(14, 0, _T("宋体"));
    settextcolor(RGB(80, 80, 80));
    outtextxy(panelX, yOffset, _T("提示：先选中机器人，再选货架/格点"));
    yOffset += 20;
    outtextxy(panelX, yOffset, _T("ESC 退出程序"));
    yOffset += 30;

    auto now = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    long long minutes = elapsedSeconds / 60;
    long long seconds = elapsedSeconds % 60;

    settextstyle(16, 0, _T("宋体"));
    settextcolor(BLACK);
    char timerStr[50];
    sprintf_s(timerStr, "运行时间: %02lld分%02lld秒 (超过30分钟)", minutes, seconds);
    outtextxy(panelX, yOffset, _T(timerStr));
    yOffset += 30;

    settextstyle(16, 0, _T("宋体"));
    outtextxy(panelX, yOffset, _T("机器人状态："));
    yOffset += 25;

    for (const auto& r : robots) {
        std::string statusStr;
        switch (r.status) {
        case RobotStatus::IDLE:    statusStr = "空闲"; break;
        case RobotStatus::MOVING:  statusStr = "移动中"; break;
        case RobotStatus::LOADING: statusStr = "装载"; break;
        case RobotStatus::ERROR_:  statusStr = "故障"; break;
        default:                   statusStr = "未知"; break;
        }

        char info[100];
        sprintf_s(info, "ID: %d  位置: (%d, %d)  状态: %s",
            r.id, r.currentPos.x, r.currentPos.y, statusStr.c_str());

        settextcolor(r.id % 2 == 0 ? BLUE : RED);
        outtextxy(panelX, yOffset, _T(info));
        yOffset += 22;
    }

    // 显示选中货架信息
    if (hasRack && !showDispatchButton) {
        settextcolor(BLUE);
        settextstyle(16, 0, _T("宋体"));
        char rackInfo[80];
        sprintf_s(rackInfo, "选中货架: (%d, %d)", rackPos.x, rackPos.y);
        outtextxy(panelX, yOffset, _T(rackInfo));
        yOffset += 22;
    }

    // 显示选中格点信息
    if (hasTarget && !showGridDispatchButton) {
        settextcolor(GREEN);
        settextstyle(16, 0, _T("宋体"));
        char targetInfo[80];
        sprintf_s(targetInfo, "选中格点: (%d, %d)", targetPos.x, targetPos.y);
        outtextxy(panelX, yOffset, _T(targetInfo));
        yOffset += 22;
    }

    settextcolor(BLACK);
}

bool GUI::isTimeout() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    return elapsed >= 1800;
}

void GUI::updateBlink() {
    DWORD now = GetTickCount();
    if (now - lastBlinkTime >= 500) {
        blinkVisible = !blinkVisible;
        lastBlinkTime = now;
    }
}

void GUI::drawSelectionRing(const Point& pos) {
    if (!blinkVisible) return;
    int cx = static_cast<int>(pos.x * GRID_SIZE + GRID_SIZE / 2);
    int cy = static_cast<int>(pos.y * GRID_SIZE + GRID_SIZE / 2);
    int radius = GRID_SIZE / 2 + 4;
    setlinecolor(RGB(255, 215, 0));
    setlinestyle(PS_DASH, 2);
    circle(cx, cy, radius);
    setlinestyle(PS_SOLID, 1);
}

void GUI::drawRackSelectionRing(const Point& pos) {
    if (!blinkVisible) return;
    int cx = static_cast<int>(pos.x * GRID_SIZE + GRID_SIZE / 2);
    int cy = static_cast<int>(pos.y * GRID_SIZE + GRID_SIZE / 2);
    int radius = GRID_SIZE / 2 + 4;
    setlinecolor(BLUE);
    setlinestyle(PS_DASH, 2);
    circle(cx, cy, radius);
    setlinestyle(PS_SOLID, 1);
}

void GUI::drawTargetSelectionRing(const Point& pos) {
    if (!blinkVisible) return;
    int cx = static_cast<int>(pos.x * GRID_SIZE + GRID_SIZE / 2);
    int cy = static_cast<int>(pos.y * GRID_SIZE + GRID_SIZE / 2);
    int radius = GRID_SIZE / 2 + 4;
    setlinecolor(GREEN);
    setlinestyle(PS_DASH, 2);
    circle(cx, cy, radius);
    setlinestyle(PS_SOLID, 1);
}

void GUI::drawDispatchButton() {
    // 右侧按钮位置
    int panelX = MAP_LENGTH * GRID_SIZE + 20;
    int btnY = 350;  // 固定位置，避免侧边栏漂移
    int btnWidth = 240;
    int btnHeight = 40;

    dispatchButtonRect.left = panelX;
    dispatchButtonRect.top = btnY;
    dispatchButtonRect.right = panelX + btnWidth;
    dispatchButtonRect.bottom = btnY + btnHeight;

    // 绘制按钮背景
    setfillcolor(RGB(70, 130, 180));
    setlinecolor(RGB(30, 80, 130));
    fillrectangle(dispatchButtonRect.left, dispatchButtonRect.top,
        dispatchButtonRect.right, dispatchButtonRect.bottom);

    // 绘制按钮文字
    settextcolor(WHITE);
    settextstyle(14, 0, _T("宋体"));
    setbkmode(TRANSPARENT);
    int textWidth = textwidth(_T(dispatchButtonText.c_str()));
    int textHeight = textheight(_T(dispatchButtonText.c_str()));
    int textX = dispatchButtonRect.left + (btnWidth - textWidth) / 2;
    int textY = dispatchButtonRect.top + (btnHeight - textHeight) / 2;
    outtextxy(textX, textY, _T(dispatchButtonText.c_str()));
}

void GUI::drawGridDispatchButton() {
    int panelX = MAP_LENGTH * GRID_SIZE + 20;
    int btnWidth = 240;
    int btnHeight = 40;
    int btnY = 350;

    if (showDispatchButton) {
        btnY = dispatchButtonRect.bottom + 10;
    }

    gridDispatchButtonRect.left = panelX;
    gridDispatchButtonRect.top = btnY;
    gridDispatchButtonRect.right = panelX + btnWidth;
    gridDispatchButtonRect.bottom = btnY + btnHeight;

    setfillcolor(RGB(60, 160, 90));
    setlinecolor(RGB(30, 110, 60));
    fillrectangle(gridDispatchButtonRect.left, gridDispatchButtonRect.top,
        gridDispatchButtonRect.right, gridDispatchButtonRect.bottom);

    settextcolor(WHITE);
    settextstyle(14, 0, _T("宋体"));
    setbkmode(TRANSPARENT);
    int textWidth = textwidth(_T(gridDispatchButtonText.c_str()));
    int textHeight = textheight(_T(gridDispatchButtonText.c_str()));
    int textX = gridDispatchButtonRect.left + (btnWidth - textWidth) / 2;
    int textY = gridDispatchButtonRect.top + (btnHeight - textHeight) / 2;
    outtextxy(textX, textY, _T(gridDispatchButtonText.c_str()));
}

void GUI::addPopup(const std::string& text, float duration) {
    popups.push_back({ text, duration });
}

void GUI::updatePopups(DWORD deltaTime) {
    float deltaSeconds = deltaTime / 1000.0f;
    auto it = popups.begin();
    while (it != popups.end()) {
        it->lifetime -= deltaSeconds;
        if (it->lifetime <= 0.0f)
            it = popups.erase(it);
        else
            ++it;
    }
}

void GUI::drawPopups() {
    if (popups.empty()) return;
    settextstyle(18, 0, _T("微软雅黑"));
    setbkmode(TRANSPARENT);

    int baseY = 100;
    for (const auto& p : popups) {
        int textWidth = textwidth(_T(p.text.c_str())) + 40;
        int textHeight = 35;
        int left = (WIN_WIDTH - textWidth) / 2;
        int top = baseY;

        setfillcolor(RGB(255, 255, 220));
        setlinecolor(RGB(200, 200, 100));
        fillrectangle(left, top, left + textWidth, top + textHeight);

        settextcolor(RGB(180, 60, 60));
        outtextxy(left + 20, top + 8, _T(p.text.c_str()));

        baseY += textHeight + 10;
    }
}