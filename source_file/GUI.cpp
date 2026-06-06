#include "GUI.h"
#include <cmath>
#include <iostream>
#include <windows.h> // Windows API 头文件

GUI::GUI(int l, int w) : screenLength(l), screenWideth(w) {
	initgraph(l, w, EX_SHOWCONSOLE);
	setbkcolor(WHITE);
	BeginBatchDraw(); // 开启双缓冲，彻底消除界面闪烁

	startTime = std::chrono::steady_clock::now();
	lastTick = GetTickCount();
	lastBlinkTime = GetTickCount();

	dispatchButtonText = "货架业务派送";
	gridDispatchButtonText = "普通格点派送";
}

GUI::~GUI() {
	closegraph();
}
/**
* @brief 业务功能分区标定染色绘制，并刚性填满打印业务文字/序号
*/
void GUI::drawSystemZones(const WarehouseManager& manager) {
	// =================================================================
	// 🌟 核心准备：配置字号以尽量填满格点 (GRID_SIZE)
	// =================================================================
	// 假设 GRID_SIZE 是你的格子像素大小（如 30 或 40）
	// 为了不让文字贴边显得太拥挤，字号设为 GRID_SIZE 的 85% 到 90% 最完美，这里直接用 GRID_SIZE - 4
	int fontSize = GRID_SIZE - 4;
	settextstyle(fontSize, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
	setbkmode(TRANSPARENT);

	// --------- 1. 标定 4 辆小车的初始老家 (🏠 淡黄色) + 标注数字 1,2,3,4 ---------
	setfillcolor(RGB(255, 255, 200)); // 淡黄色
	setlinecolor(RGB(240, 240, 180));
	settextcolor(RGB(180, 140, 30));  // 深金黄色文字，保证高对比度清晰度

	for (int id = 1; id <= 4; ++id) {
		int homeX = 0;
		int homeY = 7 + id; // Y = 8, 9, 10, 11

		int left = homeX * GRID_SIZE + 1;
		int top = homeY * GRID_SIZE + 1;
		int right = (homeX + 1) * GRID_SIZE - 1;
		int bottom = (homeY + 1) * GRID_SIZE - 1;
		fillrectangle(left, top, right, bottom);

		// 格式化数字字符串
		char idStr[4];
		sprintf_s(idStr, "%d", id);

		// 【居中核心算法】：根据当前字号动态计算文字的物理宽高，精准算射居中偏置
		int textW = textwidth(_T(idStr));
		int textH = textheight(_T(idStr));
		int textX = left + (GRID_SIZE - textW) / 2;
		int textY = top + (GRID_SIZE - textH) / 2;

		outtextxy(textX, textY, _T(idStr));
	}

	// --------- 2. 标定新大门门禁排队直道 (🛑 浅红色) + 标注“排队通道” ---------
	setfillcolor(RGB(255, 225, 225)); // 浅红色
	setlinecolor(RGB(245, 200, 200));
	settextcolor(RGB(220, 60, 60));   // 深红色汉字

	// “排队通道”四个字，对应的 X 坐标刚好是 15, 16, 17, 18
	const char* queueTexts[] = { "排", "队", "通", "道" };

	for (int i = 0; i < 4; ++i) {
		int px = 15 + i; // 15, 16, 17, 18
		int py = 10;

		int left = px * GRID_SIZE + 1;
		int top = py * GRID_SIZE + 1;
		int right = (px + 1) * GRID_SIZE - 1;
		int bottom = (py + 1) * GRID_SIZE - 1;
		fillrectangle(left, top, right, bottom);

		// 居中打字
		int textW = textwidth(_T(queueTexts[i]));
		int textH = textheight(_T(queueTexts[i]));
		int textX = left + (GRID_SIZE - textW) / 2;
		int textY = top + (GRID_SIZE - textH) / 2;

		outtextxy(textX, textY, _T(queueTexts[i]));
	}

	// --------- 3. 标定侧边返回专用离场道 (浅紫色) + 标注“返回通道” ---------
	setfillcolor(RGB(238, 224, 252)); // 浅紫色
	setlinecolor(RGB(220, 200, 240));
	settextcolor(RGB(140, 70, 210));  // 深紫色汉字

	// “返回通道”四个字，对应的 X 坐标也是 15, 16, 17, 18
	// 注意：由于返回车流是从右往左开（18 -> 15），汉字顺序可以顺着由左到右自然排布
	const char* returnTexts[] = { "返", "回", "通", "道" };

	for (int i = 0; i < 4; ++i) {
		int px = 15 + i; // 15, 16, 17, 18
		int py = 9;

		int left = px * GRID_SIZE + 1;
		int top = py * GRID_SIZE + 1;
		int right = (px + 1) * GRID_SIZE - 1;
		int bottom = (py + 1) * GRID_SIZE - 1;
		fillrectangle(left, top, right, bottom);

		// 居中打字
		int textW = textwidth(_T(returnTexts[i]));
		int textH = textheight(_T(returnTexts[i]));
		int textX = left + (GRID_SIZE - textW) / 2;
		int textY = top + (GRID_SIZE - textH) / 2;

		outtextxy(textX, textY, _T(returnTexts[i]));
	}
}
void GUI::render(const WarehouseManager& manager) {
	cleardevice();
	drawGrid();
	drawSystemZones(manager); // 绘制业务功能分区的底层色块
	manager.getMap().draw();
	// =================================================================
	// 货架编号刚性满格居中叠加
	// =================================================================
	// 根据你的 Map.cpp，i 对应 MAP_LENGTH (X)，j 对应 MAP_WIDTH (Y)
	int fontSize = GRID_SIZE - 6; // 字体略小于格子，饱满而不溢出
	settextstyle(fontSize, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
	setbkmode(TRANSPARENT);
	settextcolor(RGB(255, 255, 240)); // 象牙白文字，在灰色货架背景上对比度极高

	for (int i = 0; i < MAP_LENGTH; ++i) {
		for (int j = 0; j < MAP_WIDTH; ++j) {

			// 只有当这一格是货架（类型为 1）时，才去查表打编号
			if (manager.getMap().getType(i, j) == 1) {

				// 刚性通过管理器查表获取该格子绑定的站点 ID
				int rackId = manager.getStationIdByGrid(i, j);

				if (rackId != -1) {
					// 格式化编号
					char idStr[10];
					sprintf_s(idStr, "%d", rackId);

					// 严格对齐 Map.cpp 的物理格子矩形坐标 (面层矩形 left+3, top+3)
					int left = i * GRID_SIZE + 3;
					int top = j * GRID_SIZE + 3;
					int r_width = GRID_SIZE - 5;  // 计算面层矩形的实际宽度 (right-2 - (left+3))
					int r_height = GRID_SIZE - 5; // 计算面层矩形的实际高度 (bottom-2 - (top+3))

					// 高精居中偏置算法
					int textW = textwidth(_T(idStr));
					int textH = textheight(_T(idStr));
					int textX = left + (r_width - textW) / 2;
					int textY = top + (r_height - textH) / 2;

					// 打印货架编号
					outtextxy(textX, textY, _T(idStr));
				}
			}
		}
	}
	// =================================================================
	for (const auto& r : manager.getRobots()) {
		// 【核心显示变色逻辑】：车上有货物的时候，AGV小车变为蓝色，其余时间段为红色，清晰区分载货状态
		if (r.status == RobotStatus::LOADING || r.status == RobotStatus::MOVING_TO_DELIVER) {
			setfillcolor(BLUE);
		}
		else {
			setfillcolor(RED);
		}

		// 使用 r.realX / r.realY 绘制更平滑的实际位置
		int px = static_cast<int>(r.realX * GRID_SIZE) + GRID_SIZE / 2;
		int py = static_cast<int>(r.realY * GRID_SIZE) + GRID_SIZE / 2;
		fillcircle(px, py, GRID_SIZE / 3);
		// ==================================================
		// 项目是 ANSI 多字节字符集环境，
		// ==================================================
		char idStr[10];
		sprintf_s(idStr, "%d", r.id); // 纯正的 ANSI 字符串格式化
		settextcolor(WHITE);
		setbkmode(TRANSPARENT);
		settextstyle(16, 0, _T("微软雅黑"));
		outtextxy(px - 5, py - 8, idStr);
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

	drawStatusPanel(manager, hasSelectedRack, selectedRackPos, hasSelectedTarget, selectedTargetPos);

	// 右侧按钮
	if (showDispatchButton) {
		drawDispatchButton();
	}
	if (showGridDispatchButton) {
		drawGridDispatchButton();
	}
	// ==================================================
	// 【时序清理】： main.cpp 已经在外层更新了 updatePopups，
	// 这里只需要专注于单纯的绘制 lastTick 差值计算即可，或者统一保留一处。
	// 为了最稳妥的防御，我们直接用外部传入的值或者保留纯绘制。
	// ==================================================
	DWORD currentTick = GetTickCount();
	DWORD delta = static_cast<DWORD>(currentTick - lastTick);
	lastTick = currentTick;
	// 如果 main.cpp 传了 deltaTime，这里可以不再重复调用 updatePopups 避免双重扣减
	// 为了兼容性，我们让它只在此处安心 draw
	drawPopups();

	FlushBatchDraw(); // 将当前帧缓冲区画面一次性推送到屏幕，防止闪烁
}
/**
* @brief 处理鼠标点击事件，支持选车、选货架【现场动态下发WMS订单】以及普通格点调试派送
*/
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
				// 【O(1) 极速查表】通过货架物理格点获取对应的站点ID
				int targetStationId = manager.getStationIdByGrid(selectedRackPos.x, selectedRackPos.y);

				if (targetStationId != -1) {
					// 🚀 a. 现场印单：由于增补了非 const 重载，这一行将完美编译通过，毫无报错！
					SKUType SKU;
					int dist = abs(selectedRackPos.x - 19) + abs(selectedRackPos.y - 10);		//根据货架位置计算曼哈顿距离热度，动态分配 ABC 类 SKU
					if (dist <= 12) {
						SKU = SKUType::A;
					}
					else if (dist <= 20) {
						SKU = SKUType::B;
					}
					else {
						SKU = SKUType::C;
					}
					Order* newOrder = manager.getOrderSystem().createNewOrder(targetStationId, SKU);

					if (newOrder != nullptr) {
						// 🚀 b. 强行改变订单状态为“执行中”
						newOrder->status = OrderStatus::PROCESSING;

						// 🚀 c. 顺藤摸瓜找到选中的那辆小车，改变状态机进行深度绑定
						for (auto& r : const_cast<std::vector<Robot>&>(manager.getRobots())) {
							if (r.id == selectedRobotId) {
								r.status = RobotStatus::MOVING_TO_PICK; // 小车出发去货架取货
								r.currentOrderId = newOrder->orderId;
								r.currentTargetStationId = targetStationId;
								break;
							}
						}

						// 🚀 d. 驱动底层 RCS 动力马达：规划去货架取货的刚性路径
						manager.dispatchRobot(selectedRobotId, targetStationId);

						char buf[120];
						sprintf_s(buf, "WMS动态落单：成功生成订单#%d，小车 %d 已全速前往货架 %d！",
							newOrder->orderId, selectedRobotId, targetStationId);
						addPopup(buf);
					}
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

		// 3. 点击地图网格区域 (保持原样)
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
/**
* @brief 绘制右侧状态面板，显示系统信息、机器人状态以及实时订单监控等内容
*/
void GUI::drawStatusPanel(const WarehouseManager& manager, bool hasRack, Point rackPos, bool hasTarget, Point targetPos) {
	settextcolor(BLACK);
	settextstyle(20, 0, _T("微软雅黑"));

	int panelX = MAP_LENGTH * GRID_SIZE + 20;
	int yOffset = 20;

	outtextxy(panelX, yOffset, _T("Smart_Warehouse 系统"));
	yOffset += 30;

	settextstyle(14, 0, _T("宋体"));
	settextcolor(RGB(80, 80, 80));
	outtextxy(panelX, yOffset, _T("ESC 退出程序"));
	yOffset += 20;

	auto now = std::chrono::steady_clock::now();
	auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
	long long minutes = elapsedSeconds / 60;
	long long seconds = elapsedSeconds % 60;

	settextstyle(16, 0, _T("宋体"));
	settextcolor(BLACK);
	char timerStr[50];
	sprintf_s(timerStr, "运行时间: %02lld分%02lld秒", minutes, seconds);
	outtextxy(panelX, yOffset, _T(timerStr));
	yOffset += 30;

	settextstyle(16, 0, _T("宋体"));
	outtextxy(panelX, yOffset, _T("机器人状态："));
	yOffset += 25;

	for (const auto& r : manager.getRobots()) {
		std::string statusStr;
		switch (r.status) {
		case RobotStatus::IDLE:           statusStr = "空闲"; break;
		case RobotStatus::MOVING_TO_PICK: statusStr = "前往取货"; break;
		case RobotStatus::LOADING:        statusStr = "装载中"; break;
		case RobotStatus::MOVING_TO_DELIVER:statusStr = "前往出货"; break;
		case RobotStatus::UNLOADING:      statusStr = "卸货中"; break;
		case RobotStatus::ERROR_:         statusStr = "故障"; break;
		default:                          statusStr = "未知"; break;
		}

		char info[100];
		sprintf_s(info, "ID:%d 位置:(%d,%d) %s",
			r.id, r.currentPos.x, r.currentPos.y, statusStr.c_str());

		settextcolor(BLACK);
		outtextxy(panelX, yOffset, _T(info));
		yOffset += 22;
	}

	// 选中提示占位...
	if (hasRack && !showDispatchButton) {
		settextcolor(BLUE);
		char rackInfo[80];
		sprintf_s(rackInfo, "选中货架: (%d, %d)", rackPos.x, rackPos.y);
		outtextxy(panelX, yOffset, _T(rackInfo));
	}
	yOffset += 22;

	// === 实时订单显示区域（从 y=450 画起）===
	yOffset = 450;
	settextstyle(16, 0, _T("微软雅黑"));
	settextcolor(BLACK);
	outtextxy(panelX, yOffset, _T("【 实时业务订单监控 (WMS) 】"));
	yOffset += 30;

	auto orders = manager.getOrderSystem().getAllOrders();
	if (orders.empty()) {
		settextstyle(14, 0, _T("宋体"));
		settextcolor(RGB(150, 150, 150));
		outtextxy(panelX, yOffset, _T("暂无排队订单..."));
	}
	else {
		settextstyle(14, 0, _T("宋体"));
		int max_display = 10; // 侧边栏最多同时显示10条，防止越界

		for (size_t i = 0; i < orders.size() && i < max_display; ++i) {
			const auto& o = orders[i];
			std::string stateStr;
			if (o.status == OrderStatus::WAITING) stateStr = "等待分配>";
			else if (o.status == OrderStatus::PROCESSING) stateStr = "小车执行中>";
			else stateStr = "已完成>";

			std::string skuStr = (o.sku == SKUType::A) ? "A" : ((o.sku == SKUType::B) ? "B" : "C");

			char orderText[100];
			sprintf_s(orderText, "单源#%d | 站点:%d | SKU:%s | %s",
				o.orderId, o.targetStationId, skuStr.c_str(), stateStr.c_str());

			// 依据单源状态染色
			if (o.status == OrderStatus::WAITING) settextcolor(RGB(100, 100, 100));
			else if (o.status == OrderStatus::PROCESSING) settextcolor(RGB(20, 120, 200));
			else settextcolor(GREEN);

			outtextxy(panelX, yOffset, _T(orderText));
			yOffset += 20;
		}

		if (orders.size() > max_display) {
			settextcolor(RGB(150, 150, 150));
			outtextxy(panelX, yOffset, _T("... 更多订单折叠中"));
		}
	}
}

bool GUI::isTimeout() const {
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
	return elapsed >= 1800;		//限时30min
}

void GUI::updateBlink() {
	DWORD now = GetTickCount();
	if (now - lastBlinkTime >= 500) {
		blinkVisible = !blinkVisible;
		lastBlinkTime = now;
	}
}
/**
* @brief 绘制选中小车的闪烁环，增强视觉反馈，提示用户当前选中的机器人位置
*/
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
/**
* @brief 绘制货架选中虚线环，蓝色，闪烁提示
*/
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
/**
* @brief 绘制目标选中虚线环，绿色，闪烁提示
*/
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
/**
* @brief 绘制右侧“货架业务派送”按钮，点击后会根据选中的货架位置现场生成订单并驱动小车前往取货
*/
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
/**
* @brief 绘制普通格点派送按钮，点击后会根据选中的格点位置现场生成订单并驱动小车前往取货
*/
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
/**
* @brief 更新弹窗列表，减少每个弹窗的剩余生命周期，并移除已过期的弹窗
*/
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