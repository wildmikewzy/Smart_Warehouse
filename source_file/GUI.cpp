#include "GUI.h"
#include <cmath>
#include <iostream>

GUI::GUI(int l, int w) : screenLength(l), screenWideth(w) {
    initgraph(l, w, EX_SHOWCONSOLE);
    setbkcolor(WHITE);
    startTime = std::chrono::steady_clock::now();
    lastTick = GetTickCount();
    lastBlinkTime = GetTickCount();
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
        int px = static_cast<int>(r.currentPos.x) * GRID_SIZE + GRID_SIZE / 2;
        int py = static_cast<int>(r.currentPos.y) * GRID_SIZE + GRID_SIZE / 2;
        fillcircle(px, py, GRID_SIZE / 3);

        char idStr[10];
        sprintf_s(idStr, "%d", r.id);
        settextcolor(WHITE);
        setbkmode(TRANSPARENT);
        settextstyle(16, 0, _T("微软雅黑"));
        outtextxy(px - 5, py - 8, _T(idStr));
    }

    // 绘制选中闪烁光环（机器人或货架）—— 现在可以同时存在
    if (selectedRobotId != -1 || hasSelectedRack) {
        updateBlink();
        if (selectedRobotId != -1) {
            const auto& robots = manager.getRobots();
            for (const auto& r : robots) {
                if (r.id == selectedRobotId) {
                    drawSelectionRing(r.currentPos);
                    break;
                }
            }
        }
        if (hasSelectedRack) {
            drawRackSelectionRing(selectedRackPos);
        }
    }

    drawStatusPanel(manager.getRobots(), hasSelectedRack, selectedRackPos);

    // 绘制派送按钮（位于右侧面板内）
    if (showDispatchButton) {
        drawDispatchButton();
    }

    DWORD currentTick = GetTickCount();
    DWORD delta = currentTick - lastTick;
    lastTick = currentTick;
    updatePopups(delta);
    drawPopups();
}

void GUI::handleMouseClick(WarehouseManager& manager) {
    while (MouseHit()) {
        MOUSEMSG msg = GetMouseMsg();
        if (msg.uMsg == WM_LBUTTONDOWN) {
            int mouseX = msg.x;
            int mouseY = msg.y;

            // 1. 优先检查按钮点击
            if (showDispatchButton) {
                if (mouseX >= dispatchButtonRect.left && mouseX <= dispatchButtonRect.right &&
                    mouseY >= dispatchButtonRect.top && mouseY <= dispatchButtonRect.bottom) {
                    // 执行派发任务
                    const auto& robots = manager.getRobots();
                    bool robotExists = false;
                    for (const auto& r : robots) {
                        if (r.id == selectedRobotId) {
                            robotExists = true;
                            break;
                        }
                    }
                    if (robotExists && hasSelectedRack) {
                        std::cout << "派发机器人" << selectedRobotId << "号前往货架("
                            << selectedRackPos.x << "," << selectedRackPos.y << ")" << std::endl;
                        manager.dispatchRobot(selectedRobotId, selectedRackPos);
                        addPopup("已派发机器人" + std::to_string(selectedRobotId) + "号前往货架("
                            + std::to_string((int)selectedRackPos.x) + "," + std::to_string((int)selectedRackPos.y) + ")");
                    }
                    // 清除选中和按钮
                    selectedRobotId = -1;
                    hasSelectedRack = false;
                    showDispatchButton = false;
                    continue; // 处理完按钮点击，跳过后续地图点击逻辑
                }
            }

            // 2. 检查是否点击在地图区域
            if (mouseX >= 0 && mouseX < MAP_LENGTH * GRID_SIZE &&
                mouseY >= 0 && mouseY < MAP_WIDTH * GRID_SIZE) {

                int gridX = mouseX / GRID_SIZE;
                int gridY = mouseY / GRID_SIZE;

                // 2.1 检测是否点击了机器人
                bool hitRobot = false;
                int hitRobotId = -1;
                for (const auto& r : manager.getRobots()) {
                    float dx = r.currentPos.x - static_cast<float>(gridX);
                    float dy = r.currentPos.y - static_cast<float>(gridY);
                    if (fabs(dx) < 0.6f && fabs(dy) < 0.6f) {
                        hitRobot = true;
                        hitRobotId = r.id;
                        break;
                    }
                }

                if (hitRobot) {
                    // 如果之前已经组合选中（机器人+货架），且点击了不同的机器人，则全部取消再选中新机器人
                    if (selectedRobotId != -1 && selectedRobotId != hitRobotId && hasSelectedRack) {
                        hasSelectedRack = false;
                        showDispatchButton = false;
                    }
                    selectedRobotId = hitRobotId;
                    hasSelectedRack = false;   // 默认清掉货架选中
                    showDispatchButton = false;
                    blinkVisible = true;
                    lastBlinkTime = GetTickCount();
                    std::cout << "选中机器人" << hitRobotId << "号" << std::endl;
                    addPopup("已选中机器人 " + std::to_string(hitRobotId) + " 号");
                    continue;
                }

                // 2.2 检测是否点击了货架
                if (manager.getMap().getType(gridX, gridY) == 1) {
                    // 情况A：已经选中了一个机器人
                    if (selectedRobotId != -1) {
                        // 查找该机器人状态
                        const auto& robots = manager.getRobots();
                        bool robotIdle = false;
                        for (const auto& r : robots) {
                            if (r.id == selectedRobotId && r.status == RobotStatus::IDLE) {
                                robotIdle = true;
                                break;
                            }
                        }

                        if (robotIdle) {
                            // 机器人空闲，形成组合选中
                            selectedRackPos = { static_cast<float>(gridX), static_cast<float>(gridY) };
                            hasSelectedRack = true;
                            showDispatchButton = true;
                            dispatchButtonText = "将机器人" + std::to_string(selectedRobotId)
                                + "号派往(" + std::to_string(gridX) + "," + std::to_string(gridY) + ")货架";
                            std::cout << "将机器人" << selectedRobotId << "号派往("
                                << gridX << "," << gridY << ")货架" << std::endl;
                            addPopup("已选中货架 (" + std::to_string(gridX) + ", " + std::to_string(gridY) + ")");
                            blinkVisible = true;
                            lastBlinkTime = GetTickCount();
                        }
                        else {
                            // 机器人正在移动，提示不可派送
                            std::cout << "机器人正在派送订单，无法指定新货架" << std::endl;
                            addPopup("机器人正在派送订单");
                            // 不清除选中，但不激活货架选中
                        }
                    }
                    else {
                        // 没有选中机器人，只选中货架
                        selectedRackPos = { static_cast<float>(gridX), static_cast<float>(gridY) };
                        hasSelectedRack = true;
                        selectedRobotId = -1;   // 确保机器人取消
                        showDispatchButton = false;
                        blinkVisible = true;
                        lastBlinkTime = GetTickCount();
                        std::cout << "选中货架 (" << gridX << ", " << gridY << ")" << std::endl;
                        addPopup("已选中货架 (" + std::to_string(gridX) + ", " + std::to_string(gridY) + ")");
                    }
                    continue;
                }

                // 2.3 点击空地（既非机器人也非货架）
                // 如果此时有组合选中，点击空地会让其消失
                if (selectedRobotId != -1 || hasSelectedRack) {
                    if (selectedRobotId != -1 && hasSelectedRack) {
                        std::cout << "已取消机器人与货架的组合选中" << std::endl;
                    }
                    selectedRobotId = -1;
                    hasSelectedRack = false;
                    showDispatchButton = false;
                    addPopup("已取消选中");
                }
                else {
                    addPopup("未选中任何目标，请点击机器人或货架");
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

void GUI::drawStatusPanel(const std::vector<Robot>& robots, bool hasRack, Point rackPos) {
    settextcolor(BLACK);
    settextstyle(20, 0, _T("微软雅黑"));

    int panelX = MAP_LENGTH * GRID_SIZE + 20;
    int yOffset = 20;

    outtextxy(panelX, yOffset, _T("Smart_Warehouse 系统"));
    yOffset += 30;

    settextstyle(14, 0, _T("宋体"));
    settextcolor(RGB(80, 80, 80));
    outtextxy(panelX, yOffset, _T("点击机器人或货架可选中，点击空地取消"));
    yOffset += 20;
    outtextxy(panelX, yOffset, _T("按键 N / M 派发机器人任务"));
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
    sprintf_s(timerStr, "运行时间: %02lld分%02lld秒 (上限30分钟)", minutes, seconds);
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
        case RobotStatus::ERROR_:  statusStr = "错误"; break;
        default:                   statusStr = "未知"; break;
        }

        char info[100];
        sprintf_s(info, "ID: %d  位置: (%.0f, %.0f)  状态: %s",
            r.id, r.currentPos.x, r.currentPos.y, statusStr.c_str());

        settextcolor(r.id % 2 == 0 ? BLUE : RED);
        outtextxy(panelX, yOffset, _T(info));
        yOffset += 22;
    }

    // 显示选中的货架信息（仅当没有显示按钮时，避免重复）
    if (hasRack && !showDispatchButton) {
        settextcolor(BLUE);
        settextstyle(16, 0, _T("宋体"));
        char rackInfo[80];
        sprintf_s(rackInfo, "选中货架: (%.0f, %.0f)", rackPos.x, rackPos.y);
        outtextxy(panelX, yOffset, _T(rackInfo));
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

void GUI::drawDispatchButton() {
    // 定义按钮位置（右侧面板内）
    int panelX = MAP_LENGTH * GRID_SIZE + 20;
    int btnY = 350;  // 固定位置，避免随列表变动
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