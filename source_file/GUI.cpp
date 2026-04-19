#include "GUI.h"
#include <iostream>

GUI::GUI(int l, int w) : screenLength(l), screenWideth(w) {
    initgraph(l, w, EX_SHOWCONSOLE);
    setbkcolor(WHITE);

    // 初始化按钮位置（放在状态栏下方）
	int panelX = MAP_LENGTH * GRID_SIZE + 20;       //统一状态栏和按钮的X坐标
    btn1 = { panelX, 400, 160, 40, false, "分派机器人1号" };
    btn2 = { panelX, 450, 160, 40, false, "分派机器人2号" };
}

GUI::~GUI() {
    closegraph();
}
/**
* @brief 渲染
*/
void GUI::render(const WarehouseManager& manager) {
	cleardevice();      // 清屏函数
	drawGrid();         // 画网格线

    // 画地图上的障碍物
    manager.getMap().draw();

    // 画机器人
    for (const auto& r : manager.getRobots()) {
        // --- 绘制机器人的轨迹线 ---
        if (r.historyPath.size() > 1) {
			setlinecolor(r.id % 2 == 0 ? LIGHTBLUE : LIGHTRED);     // 根据机器人ID 其路径选择不同的颜色，偶数ID使用浅蓝色，奇数ID使用浅红色
            setlinestyle(PS_DOT, 2);

			for (size_t i = 0; i < r.historyPath.size() - 1; i++) {     // 将历史路径点转换为像素坐标并绘制线段
                int x1 = static_cast<int>(r.historyPath[i].x) * GRID_SIZE + GRID_SIZE / 2;
                int y1 = static_cast<int>(r.historyPath[i].y) * GRID_SIZE + GRID_SIZE / 2;
                int x2 = static_cast<int>(r.historyPath[i + 1].x) * GRID_SIZE + GRID_SIZE / 2;
                int y2 = static_cast<int>(r.historyPath[i + 1].y) * GRID_SIZE + GRID_SIZE / 2;
                line(x1, y1, x2, y2);
            }
        }
        //绘制机器人圆形本体
        setfillcolor(r.id % 2 == 0 ? BLUE : RED);     // 根据机器人ID选择不同的颜色，偶数ID使用蓝色，奇数ID使用红色
        int px = static_cast<int>(r.currentPos.x) * GRID_SIZE + GRID_SIZE / 2;
        int py = static_cast<int>(r.currentPos.y) * GRID_SIZE + GRID_SIZE / 2;
		fillcircle(px, py, GRID_SIZE / 3);      // 绘制机器人主体圆形
        //==绘制机器人编号==
		char idStr[10];
		sprintf_s(idStr, "%d", r.id);       //数字转字符串，准备绘制在机器人上
		settextcolor(WHITE);        // 设置文本颜色为白色，以便在机器人圆形上清晰显示
		setbkmode(TRANSPARENT);     // 设置文本背景模式为透明，这样文本不会覆盖机器人圆形的颜色
        settextstyle(16, 0, _T("微软雅黑"));        //设置字体和大小
		outtextxy(px-5, py-8, _T(idStr));     // 在机器人圆形上方绘制编号文本    

    }

    drawStatusPanel(manager.getRobots());
    drawButtons(); // 渲染下拉列新增按钮
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
        std::string statusStr;
        switch (r.status) {
            case RobotStatus::IDLE:    statusStr = "空闲"; break;
		    case RobotStatus::PREPARING: statusStr = "准备中"; break;
            case RobotStatus::MOVING:  statusStr = "移动中"; break;
            case RobotStatus::LOADING: statusStr = "装载"; break;
            case RobotStatus::ERROR_:  statusStr = "错误"; break;
            default:statusStr = "未知"; break;
        }

        char infoLine1[100];
        char infoLine2[100];
        sprintf_s(infoLine1, "ID: %d   位置: (%.2f, %.2f)", r.id, r.currentPos.x, r.currentPos.y);
        sprintf_s(infoLine2, "状态: %s", statusStr.c_str());

        settextcolor(r.id % 2 == 0 ? BLUE : RED);
        outtextxy(panelX, yOffset, _T(infoLine1));
        yOffset += 22;
        outtextxy(panelX, yOffset, _T(infoLine2));
        yOffset += 25;
    }
    settextcolor(BLACK); // 恢复默认
}

// 新增：绘制按钮逻辑
void GUI::drawButtons() {
    // 渲染方法：如果按钮被按下，使用较深的颜色，从而产生凹陷按下的动态效果
    auto drawBtn = [](const Button& btn, COLORREF baseColor) {
        setlinecolor(BLACK);
        // 按下时颜色变深
        setfillcolor(btn.isPressed ? RGB(180, 180, 180) : baseColor);
        
        // 如果按下，将其坐标轻微偏移做出真正的按压感
		int offset = btn.isPressed ? 2 : 0;     // offset 用于模拟按钮被按下时的视觉效果,当按钮被按下时，offset 为 2，使按钮稍稍偏移一点看起来被按压了；否则为 0，保持正常位置。
        fillrectangle(btn.x + offset, btn.y + offset, btn.x + btn.width + offset, btn.y + btn.height + offset);
        
        settextcolor(BLACK);
        settextstyle(16, 0, _T("宋体"));
		// 居中计算文本位置，简单起见这里直接加了固定偏移
        outtextxy(btn.x + 15 + offset, btn.y + 12 + offset, btn.text.c_str());
    };

    drawBtn(btn1, RGB(220, 220, 220));
    drawBtn(btn2, RGB(220, 220, 220));
}

// 修改：处理所有鼠标事件，并接收 Manager 进行任务分配
void GUI::handleMouseClick(WarehouseManager& manager) {
    // 使用 while 处理所有积压的鼠标消息，保证界面不卡顿
    while (MouseHit()) {
        MOUSEMSG msg = GetMouseMsg();
        
        // 处理鼠标按下
        if (msg.uMsg == WM_LBUTTONDOWN) {
            if (btn1.contains(msg.x, msg.y)) {
                btn1.isPressed = true;
            } else if (btn2.contains(msg.x, msg.y)) {
                btn2.isPressed = true;
            } else {
                int mapPixelWidth = MAP_LENGTH * GRID_SIZE;
                int mapPixelHeight = MAP_WIDTH * GRID_SIZE;

                if (msg.x >= 0 && msg.x < mapPixelWidth && msg.y >= 0 && msg.y < mapPixelHeight) {
                    int gridX = msg.x / GRID_SIZE;
                    int gridY = msg.y / GRID_SIZE;
                    printf("[鼠标交互] 点击地图 - 网格坐标: (%d, %d)\n", gridX, gridY);
                } else {
                    printf("[鼠标交互] 点击状态栏 - 像素坐标: (X: %d, Y: %d)\n", msg.x, msg.y);
                }
            }
        } 
        // 处理鼠标松开
        else if (msg.uMsg == WM_LBUTTONUP) {
            if (btn1.isPressed && btn1.contains(msg.x, msg.y)) {
                std::cout << "[UI交互] 点击按钮：机器人 1 号前往货架区 (6, 9)" << std::endl;
                manager.dispatchRobot(1, { 6, 9 });
            }
            if (btn2.isPressed && btn2.contains(msg.x, msg.y)) {
                std::cout << "[UI交互] 点击按钮：机器人 2 号前往货架区 (8, 9)" << std::endl;
                manager.dispatchRobot(2, { 8, 9 });
            }
            
            // 无论在哪里松开鼠标，都重置按下状态
            btn1.isPressed = false;
            btn2.isPressed = false;
        }
    }
}