#pragma once
#include <graphics.h>
#include "common.h"
#include "WarehouseManager.h"
#include <string>
#include <list>
#include <chrono>

struct PopupMessage {
    std::string text;
    float lifetime;
};

class GUI {
private:
    int screenLength;
    int screenWideth;
    std::chrono::steady_clock::time_point startTime;
    ULONGLONG lastTick;
    int selectedRobotId = -1;
    bool hasSelectedRack = false;
    Point selectedRackPos = { -1, -1 };
    bool hasSelectedTarget = false;
    Point selectedTargetPos = { -1, -1 };
    std::list<PopupMessage> popups;
    ULONGLONG lastBlinkTime;
    bool blinkVisible = true;

    // 新增：派送按钮相关
    bool showDispatchButton = false;
    RECT dispatchButtonRect;
    std::string dispatchButtonText;

    // 新增：格点派送按钮相关
    bool showGridDispatchButton = false;
    RECT gridDispatchButtonRect;
    std::string gridDispatchButtonText;

public:
    GUI(int l, int w);
    ~GUI();

    void render(const WarehouseManager& manager);
    void handleMouseClick(WarehouseManager& manager);  // 改为非const，以调用派发
    bool isTimeout() const;
    void addPopup(const std::string& text, float duration = 2.5f);
    void updatePopups(DWORD deltaTime);

private:
    void drawGrid();
    void drawStatusPanel(const WarehouseManager& manager, bool hasRack, Point rackPos, bool hasTarget, Point targetPos);
    void drawPopups();
    void updateBlink();
    void drawSelectionRing(const Point& pos);
    void drawRackSelectionRing(const Point& pos);
    void drawTargetSelectionRing(const Point& pos);
    void drawDispatchButton();   // 新增：绘制派送按钮
    void drawGridDispatchButton();
};