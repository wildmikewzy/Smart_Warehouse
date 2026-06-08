#include "GUI.h"
#include <cmath>
#include <iostream>
#include <windows.h> // Windows API 头文件

/**
 * @brief GUI 视口感知渲染引擎构造函数
 * @param l 屏幕物理视口像素宽度
 * @param w 屏幕物理视口像素高度
 */
GUI::GUI(int l, int w) : screenLength(l), screenWideth(w) {
	initgraph(l, w, EX_SHOWCONSOLE);
	setbkcolor(WHITE);
	BeginBatchDraw(); // 启用双缓冲图形渲染机制（Double-Buffered Graphics Rendering Mechanism），消除屏幕扫描撕裂与高频闪烁缺陷

	startTime = std::chrono::steady_clock::now();
	lastTick = GetTickCount();
	lastBlinkTime = GetTickCount();

	dispatchButtonText = "货架业务派送";
	gridDispatchButtonText = "普通格点派送";
}

/**
 * @brief GUI 视口静态资源释放与图形管线卸载析构函数
 */
GUI::~GUI() {
	closegraph();
}

/**
 * @brief 仓储物流系统全局静态业务功能分区拓扑染色及空间标定
 * @param manager 全局仓储调度控制核心管理器常引用
 */
void GUI::drawSystemZones(const WarehouseManager& manager) {
	// =================================================================
	// 核心准备：根据网格单位空间几何度量（GRID_SIZE）动态配置标准饱满文本字型
	// =================================================================
	// 运筹视觉准则：为避免字形拓扑溢出或视觉边缘拥挤，将文本字号约束为网格物理特征尺度的 85% - 90%
	int fontSize = GRID_SIZE - 4;
	settextstyle(fontSize, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
	setbkmode(TRANSPARENT);

	// --------- 1. 标定 4 辆智能移动体 AGV 的初始驻留泊位（Berth / Initial Depot Location）---------
	setfillcolor(RGB(255, 255, 200)); // 高饱和度空间感知黄色基底
	setlinecolor(RGB(240, 240, 180));
	settextcolor(RGB(180, 140, 30));  // 增强视见对比度专属深金黄文本色彩

	for (int id = 1; id <= 4; ++id) {
		int homeX = 0;
		int homeY = 7 + id; // 空间网格物理坐标投影 Y = 8, 9, 10, 11

		int left = homeX * GRID_SIZE + 1;
		int top = homeY * GRID_SIZE + 1;
		int right = (homeX + 1) * GRID_SIZE - 1;
		int bottom = (homeY + 1) * GRID_SIZE - 1;
		fillrectangle(left, top, right, bottom);

		// 格式化数字字符串
		char idStr[4];
		sprintf_s(idStr, "%d", id);

		// 【二维视口包围盒绝对居中投影算法】：计算当前标量字形的空间物理边框，精确计算多轴补偿偏置
		int textW = textwidth(_T(idStr));
		int textH = textheight(_T(idStr));
		int textX = left + (GRID_SIZE - textW) / 2;
		int textY = top + (GRID_SIZE - textH) / 2;

		outtextxy(textX, textY, _T(idStr));
	}

	// --------- 2. 标定上游 WMS 任务注入缓冲队列排队直道（Inbound Scheduling Buffer Queue-Lane）---------
	setfillcolor(RGB(255, 225, 225)); // 浅红低频背景标定
	setlinecolor(RGB(245, 200, 200));
	settextcolor(RGB(220, 60, 60));   // 强对比高纯度文字层

	// 空间网格离散采样映射：文本物理拓扑对应 X 轴区间 [15, 18]
	const char* queueTexts[] = { "排", "队", "通", "道" };

	for (int i = 0; i < 4; ++i) {
		int px = 15 + i; // 15, 16, 17, 18
		int py = 10;

		int left = px * GRID_SIZE + 1;
		int top = py * GRID_SIZE + 1;
		int right = (px + 1) * GRID_SIZE - 1;
		int bottom = (py + 1) * GRID_SIZE - 1;
		fillrectangle(left, top, right, bottom);

		// 边界包围盒投影居中渲染
		int textW = textwidth(_T(queueTexts[i]));
		int textH = textheight(_T(queueTexts[i]));
		int textX = left + (GRID_SIZE - textW) / 2;
		int textY = top + (GRID_SIZE - textH) / 2;

		outtextxy(textX, textY, _T(queueTexts[i]));
	}

	// --------- 3. 标定离散智能体作业完毕后返回离场周转通道（Outbound Turnaround Departure Lane）---------
	setfillcolor(RGB(238, 224, 252)); // 浅紫低频空间染色
	setlinecolor(RGB(220, 200, 240));
	settextcolor(RGB(140, 70, 210));  // 深紫文字对比层

	// 空间网格离散采样映射：多智能体逆流方向回归拓扑 X 轴区间 [15, 18]
	const char* returnTexts[] = { "返", "回", "通", "道" };

	for (int i = 0; i < 4; ++i) {
		int px = 15 + i; // 15, 16, 17, 18
		int py = 9;

		int left = px * GRID_SIZE + 1;
		int top = py * GRID_SIZE + 1;
		int right = (px + 1) * GRID_SIZE - 1;
		int bottom = (py + 1) * GRID_SIZE - 1;
		fillrectangle(left, top, right, bottom);

		// 边界包围盒投影居中渲染
		int textW = textwidth(_T(returnTexts[i]));
		int textH = textheight(_T(returnTexts[i]));
		int textX = left + (GRID_SIZE - textW) / 2;
		int textY = top + (GRID_SIZE - textH) / 2;

		outtextxy(textX, textY, _T(returnTexts[i]));
	}
}

/**
 * @brief 仓储物流推演系统图形渲染主管线周期渲染驱动函数
 * @param manager 全局仓储调度控制核心管理器常引用
 */
void GUI::render(const WarehouseManager& manager) {
	cleardevice();
	drawGrid();
	drawSystemZones(manager); // 绘制底层空间静态业务功能染色拓扑图层
	manager.getMap().draw();

	// =================================================================
	// 货架拓扑实体网格空间编号光栅化文本图层叠加渲染（Rasterized Text Layer Overlay）
	// =================================================================
	// 投影几何：基于一阶映射网络体系，i 为多轴矩阵长（X轴），j 为宽（Y轴）
	int fontSize = GRID_SIZE - 6; // 边界饱满约束，避免产生视口渲染冲突
	settextstyle(fontSize, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
	setbkmode(TRANSPARENT);
	settextcolor(RGB(255, 255, 240)); // 高视见象牙白对比色

	for (int i = 0; i < MAP_LENGTH; ++i) {
		for (int j = 0; j < MAP_WIDTH; ++j) {

			// 图形管线状态机条件过滤：仅当栅格类型解算为底层货架节点（Type = 1）时激活渲染
			if (manager.getMap().getType(i, j) == 1) {

				// 刚性通过存储池关联映射检索该空间格点物理绑定的货架站点物理 ID
				int rackId = manager.getStationIdByGrid(i, j);

				if (rackId != -1) {
					// 格式化编号
					char idStr[10];
					sprintf_s(idStr, "%d", rackId);

					// 拓扑对齐：严密匹配 Map.cpp 面层浮雕几何矩形坐标 (偏移基准 left+3, top+3)
					int left = i * GRID_SIZE + 3;
					int top = j * GRID_SIZE + 3;
					int r_width = GRID_SIZE - 5;  // 解算实际渲染表面物理横向步长
					int r_height = GRID_SIZE - 5; // 解算实际渲染表面物理纵向步长

					// 二维视口绝对居中偏置投影
					int textW = textwidth(_T(idStr));
					int textH = textheight(_T(idStr));
					int textX = left + (r_width - textW) / 2;
					int textY = top + (r_height - textH) / 2;

					// 写入光栅化位图文本图层
					outtextxy(textX, textY, _T(idStr));
				}
			}
		}
	}

	// =================================================================
	// 离散智能体多动力学状态二维欧几里得空间插值平滑渲染层
	// =================================================================
	for (const auto& r : manager.getRobots()) {
		// 依据智能体当前载荷状态执行离散颜色编码（Discrete Color Encoding Based on Agent Payload State）：
		// 当处于时空重载转运（LOADING / MOVING_TO_DELIVER）相态时映射为饱满蓝色，闲置或轻载寻路相态映射为标准红色
		if (r.status == RobotStatus::LOADING || r.status == RobotStatus::MOVING_TO_DELIVER) {
			setfillcolor(BLUE);
		}
		else {
			setfillcolor(RED);
		}

		// 核心平滑机制：利用动力学周期内高频更新的亚像素实数物理位置参数（r.realX / r.realY）执行欧氏空间线性投影插值
		int px = static_cast<int>(r.realX * GRID_SIZE) + GRID_SIZE / 2;
		int py = static_cast<int>(r.realY * GRID_SIZE) + GRID_SIZE / 2;
		fillcircle(px, py, GRID_SIZE / 3);

		// --------------------------------------------------
		// 多字节字符集（MBCS/ANSI）运行架构兼容性文本图层输出
		// --------------------------------------------------
		char idStr[10];
		sprintf_s(idStr, "%d", r.id); // 执行原子级纯正 ANSI 序列化格式化
		settextcolor(WHITE);
		setbkmode(TRANSPARENT);
		settextstyle(16, 0, _T("微软雅黑"));
		outtextxy(px - 5, py - 8, idStr);
	}

	// =================================================================
	// 视口交互高亮包围盒边界闪烁追踪算子（Visual Tracking Boundary Loop）
	// =================================================================
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
	// 时钟流控制：当任何实体交互标记激活时，触发时钟占空比控制波形以开启界面高频同步闪烁反馈
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

	// 驱动渲染侧边栏全局综合效能大盘监测面板
	drawStatusPanel(manager, hasSelectedRack, selectedRackPos, hasSelectedTarget, selectedTargetPos);

	// 条件流渲染：右侧操控台指令下发原子级控制视口触发
	if (showDispatchButton) {
		drawDispatchButton();
	}
	if (showGridDispatchButton) {
		drawGridDispatchButton();
	}

	// ==================================================
	// 时间步差分补偿与悬浮事件状态大盘清理
	// ==================================================
	DWORD currentTick = GetTickCount();
	DWORD delta = static_cast<DWORD>(currentTick - lastTick);
	lastTick = currentTick;
	// 架构自隔离防御性原则：渲染管线剥离多周期重复调用 updatePopups 引起的双重扣减失真，维持纯图形化输出
	drawPopups();

	FlushBatchDraw(); // 触发底盘硬件显存批处理交换，一次性强制推送至主显示视口，切断闪烁成因
}

/**
 * @brief 视口高频鼠标输入异步事件驱动响应及多模态空间坐标反投影解析算子
 * @param manager 全局仓储调度控制核心管理器引用
 */
void GUI::handleMouseClick(WarehouseManager& manager) {
	ExMessage msg;

	// 轮询非阻塞消息管道：提取当前高频捕获的鼠标事件
	while (peekmessage(&msg, EX_MOUSE)) {
		if (msg.message != WM_LBUTTONDOWN) {
			continue;
		}

		int mx = msg.x;
		int my = msg.y;
		bool clickHandled = false; // 原子状态保护锁：每次提取事件时严格重置事件消费旗帜

		// ----------------------------------------------------------------====
		// 1. 视口控制台点击事件拦截：[WMS 异步增量订单落单/分配复合操控台]（货架业务派送按钮）
		// ----------------------------------------------------------------====
		if (showDispatchButton && mx >= dispatchButtonRect.left && mx <= dispatchButtonRect.right &&
			my >= dispatchButtonRect.top && my <= dispatchButtonRect.bottom)
		{
			if (selectedRobotId != -1 && hasSelectedRack) {
				int targetStationId = manager.getStationIdByGrid(selectedRackPos.x, selectedRackPos.y);
				if (targetStationId != -1) {
					// 热度分区计算（ABC分类法）：依据曼哈顿空间测距度量判定货单物料属性与仓储流率等级
					SKUType SKU;
					int dist = abs(selectedRackPos.x - 19) + abs(selectedRackPos.y - 10);
					if (dist <= 12) SKU = SKUType::A;
					else if (dist <= 20) SKU = SKUType::B;
					else SKU = SKUType::C;

					// 触发上游 WMS 订单流异步注入与实时调度关联映射（Upstream WMS Order Stream Asynchronous Injection）
					Order* newOrder = manager.getOrderSystem().createNewOrder(targetStationId, SKU);
					if (newOrder != nullptr) {
						newOrder->status = OrderStatus::PROCESSING;
						for (auto& r : const_cast<std::vector<Robot>&>(manager.getRobots())) {
							if (r.id == selectedRobotId) {
								r.status = RobotStatus::MOVING_TO_PICK;
								r.currentOrderId = newOrder->orderId;
								r.currentTargetStationId = targetStationId;
								break;
							}
						}
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
			continue; // 事件生命周期截断：已消费交互请求，拦截后续穿透判定
		}

		// ====================================================================
		// 2. 视口控制台点击事件拦截：[人工干预绝对运动强驱超载操控台]（普通格点派送按钮）
		// ====================================================================
		if (showGridDispatchButton && mx >= gridDispatchButtonRect.left && mx <= gridDispatchButtonRect.right &&
			my >= gridDispatchButtonRect.top && my <= gridDispatchButtonRect.bottom)
		{
			if (selectedRobotId != -1 && hasSelectedTarget) {
				for (auto& r : const_cast<std::vector<Robot>&>(manager.getRobots())) {
					if (r.id == selectedRobotId) {
						r.currentOrderId = -1;
						r.status = RobotStatus::MANUAL_OVERRIDE; // 切换动力学状态至高优先级人工强驱接管模式
						r.currentTargetStationId = -1;

						// 调用底层人工干预绝对运动强驱超载算子，重写内部时空约束路径
						manager.moveRobotToGrid(selectedRobotId, selectedTargetPos.x, selectedTargetPos.y);

						char buf[80];
						sprintf_s(buf, "[强驱脱困] 已向小车 %d 下发刚性位移指令 -> (%d, %d)",
							selectedRobotId, selectedTargetPos.x, selectedTargetPos.y);
						addPopup(buf);
						break;
					}
				}
				hasSelectedTarget = false;
				showGridDispatchButton = false;
			}
			clickHandled = true;
		}

		// ====================================================================
		// 3. 视口控制台点击事件拦截：[WMS 自动接单状态机反转总线开关]
		// ====================================================================
		if (mx >= wmsToggleButtonRect.left && mx <= wmsToggleButtonRect.right &&
			my >= wmsToggleButtonRect.top && my <= wmsToggleButtonRect.bottom)
		{
			// 触发 Wms 自动接单状态机状态的一键非逻辑取反
			isWmsAutoReceiving = !isWmsAutoReceiving;

			char buf[80];
			if (isWmsAutoReceiving) {
				sprintf_s(buf, "[WMS系统] 电商前台接单已【开启】！上游订单流开始注入...");
				addPopup(buf, 3.0f);
			}
			else {
				sprintf_s(buf, "[WMS系统] 电商前台接单已【截断】！暂停接收新订单。");
				addPopup(buf, 3.0f);
			}

			clickHandled = true;
		}

		if (clickHandled) {
			continue; // 事件生命周期截断：避免穿透干扰底层物理网格拓扑
		}

		// ====================================================================
		// 4. 二维物理视口逆向几何映射：多模态空间坐标反投影解析算子（点击地图网格区域）
		// ====================================================================
		int gridX = mx / GRID_SIZE;
		int gridY = my / GRID_SIZE;

		if (gridX >= 0 && gridX < MAP_LENGTH && gridY >= 0 && gridY < MAP_WIDTH) {
			bool clickedRobot = false;
			// 判定是否逆向碰撞选中了某辆离散运动体 AGV
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

			// 在选定物理智能体的前提下，解析非碰撞背景空间的点击行为属性
			if (!clickedRobot && selectedRobotId != -1) {
				const Map& warehouseMap = manager.getMap();
				// 状态 A：若目标反投影坐标为非可行走货架栅格，判定为分配 WMS 目标货架行为
				if (!warehouseMap.isWalkable(gridX, gridY)) {
					selectedRackPos = { gridX, gridY };
					hasSelectedRack = true;
					showDispatchButton = true;
					hasSelectedTarget = false;
					showGridDispatchButton = false;
				}
				// 状态 B：若目标反投影坐标为离散通行空地栅格，判定为人为越权调试点对点强驱寻路
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

/**
 * @brief 二维离散正交网格拓扑参考系线框图层绘制
 */
void GUI::drawGrid() {
	setlinecolor(LIGHTGRAY);
	for (int i = 0; i <= MAP_LENGTH; i++)
		line(i * GRID_SIZE, 0, i * GRID_SIZE, MAP_WIDTH * GRID_SIZE);
	for (int i = 0; i <= MAP_WIDTH; i++)
		line(0, i * GRID_SIZE, MAP_LENGTH * GRID_SIZE, i * GRID_SIZE);
}

/**
 * @brief 全局系统综合效能监控看板（WMS 状态面板及侧边核心指标监测大盘）
 * @param manager 全局仓储调度控制核心管理器常引用
 * @param hasRack 当前是否含有激活选中的货架状态位
 * @param rackPos 激活货架的空间几何坐标点
 * @param hasTarget 当前是否含有激活选中的强驱目标状态位
 * @param targetPos 激活强驱目标的空间几何坐标点
 */
void GUI::drawStatusPanel(const WarehouseManager& manager, bool hasRack, Point rackPos, bool hasTarget, Point targetPos) {
	settextcolor(BLACK);
	settextstyle(20, 0, _T("微软雅黑"));

	int panelX = MAP_LENGTH * GRID_SIZE + 20; // 规避静态绝对坐标视口漂移缺陷的固定偏置边界 X 轴基准
	int yOffset = 20;

	outtextxy(panelX, yOffset, _T("Smart_Warehouse 系统"));
	yOffset += 30;

	settextstyle(14, 0, _T("宋体"));
	settextcolor(RGB(80, 80, 80));
	outtextxy(panelX, yOffset, _T("ESC 退出程序"));
	yOffset += 20;

	// 离散时序解算：统计系统自初始化运行以来的高精度绝对绝对物理耗时
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

	// 多智能体全生命周期内部相态高频字典翻译与物理投影显示
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

	// 交互式选中状态临时状态展示插位
	if (hasRack && !showDispatchButton) {
		settextcolor(BLUE);
		char rackInfo[80];
		sprintf_s(rackInfo, "选中货架: (%d, %d)", rackPos.x, rackPos.y);
		outtextxy(panelX, yOffset, _T(rackInfo));
	}
	yOffset += 22;

	// ====================================================================
	// 实时活跃订单链表监测区域（固定绝对像素坐标 y = 450 分层渲染）
	// ====================================================================
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
		int max_display = 10; // 视口纵向高度边界截断约束，防止文字相互叠置或溢出物理屏幕

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

			// 基于业务逻辑有限状态机的自适应多色光影高亮标记染色体系
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
	drawWmsToggleButton();		// 驱动渲染 WMS 自动接单开关人机交互按钮
}

/**
 * @brief 系统运行时间上限熔断硬拦截判定
 * @retval bool 若系统运行时长超越硬时间开销界限（如 30 分钟崩溃线），返回真以启动强制保护
 */
bool GUI::isTimeout() const {
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
	return elapsed >= 1800;		// 系统全局仿真最长受限周期限制（30分钟）
}

/**
 * @brief 基于脉冲时钟发生器周期的闪烁占空比反转控制算子
 */
void GUI::updateBlink() {
	DWORD now = GetTickCount();
	if (now - lastBlinkTime >= 500) { // 周期限制为 500 毫秒的反转间隔
		blinkVisible = !blinkVisible;
		lastBlinkTime = now;
	}
}

/**
 * @brief 选定物理智能体（AGV）高亮包围虚线环形算子绘制
 * @param pos 智能体当前离散正交空间坐标点
 */
void GUI::drawSelectionRing(const Point& pos) {
	if (!blinkVisible) return;
	int cx = static_cast<int>(pos.x * GRID_SIZE + GRID_SIZE / 2);
	int cy = static_cast<int>(pos.y * GRID_SIZE + GRID_SIZE / 2);
	int radius = GRID_SIZE / 2 + 4;
	setlinecolor(RGB(255, 215, 0)); // 金黄色高视见高亮
	setlinestyle(PS_DASH, 2);
	circle(cx, cy, radius);
	setlinestyle(PS_SOLID, 1);
}

/**
 * @brief 选定非通行货架栅格目标高亮包围虚线环形算子绘制
 * @param pos 货架目标当前离散正交空间坐标点
 */
void GUI::drawRackSelectionRing(const Point& pos) {
	if (!blinkVisible) return;
	int cx = static_cast<int>(pos.x * GRID_SIZE + GRID_SIZE / 2);
	int cy = static_cast<int>(pos.y * GRID_SIZE + GRID_SIZE / 2);
	int radius = GRID_SIZE / 2 + 4;
	setlinecolor(BLUE); // 科技蓝业务区标记高亮
	setlinestyle(PS_DASH, 2);
	circle(cx, cy, radius);
	setlinestyle(PS_SOLID, 1);
}

/**
 * @brief 选定空地通行网格位置高亮包围虚线环形算子绘制
 * @param pos 强驱目标当前离散正交空间坐标点
 */
void GUI::drawTargetSelectionRing(const Point& pos) {
	if (!blinkVisible) return;
	int cx = static_cast<int>(pos.x * GRID_SIZE + GRID_SIZE / 2);
	int cy = static_cast<int>(pos.y * GRID_SIZE + GRID_SIZE / 2);
	int radius = GRID_SIZE / 2 + 4;
	setlinecolor(GREEN); // 调试绿绝对强驱标记高亮
	setlinestyle(PS_DASH, 2);
	circle(cx, cy, radius);
	setlinestyle(PS_SOLID, 1);
}

/**
 * @brief 右侧业务控制台组件：“货架业务派送”交互式按钮图层绘制
 */
void GUI::drawDispatchButton() {
	int panelX = MAP_LENGTH * GRID_SIZE + 20;
	int btnY = 350;  // 绝对像素高度锚定，规避静态绝对坐标视口漂移缺陷
	int btnWidth = 240;
	int btnHeight = 40;

	dispatchButtonRect.left = panelX;
	dispatchButtonRect.top = btnY;
	dispatchButtonRect.right = panelX + btnWidth;
	dispatchButtonRect.bottom = btnY + btnHeight;

	// 按钮色彩及边界线宽状态机切换
	setfillcolor(RGB(70, 130, 180));
	setlinecolor(RGB(30, 80, 130));
	fillrectangle(dispatchButtonRect.left, dispatchButtonRect.top,
		dispatchButtonRect.right, dispatchButtonRect.bottom);

	// 光栅化文本包围盒绝对居中投影
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
 * @brief 右侧业务控制台组件：“普通格点派送”交互式按钮图层绘制
 */
void GUI::drawGridDispatchButton() {
	int panelX = MAP_LENGTH * GRID_SIZE + 20;
	int btnWidth = 240;
	int btnHeight = 40;
	int btnY = 350;

	// 多控动态布局自适应：若上层业务派送按钮同步激活，强制执行线性垂直层叠堆叠
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

/**
 * @brief 向异步事件悬浮反馈队列中增量推送高显弹窗节点
 * @param text 需要呈现的文本信息常引用
 * @param duration 该信息在视口层面的预设物理维持周期（秒）
 */
void GUI::addPopup(const std::string& text, float duration) {
	popups.push_back({ text, duration });
}

/**
 * @brief 异步事件悬浮反馈队列的时间步衰减生命周期治理与动态内存裁剪
 * @param deltaTime 主控制回路当前周期的硬件高精度差分增量时钟步长（毫秒）
 */
void GUI::updatePopups(DWORD deltaTime) {
	float deltaSeconds = deltaTime / 1000.0f;
	auto it = popups.begin();
	while (it != popups.end()) {
		it->lifetime -= deltaSeconds; // 步长线性衰减机制
		if (it->lifetime <= 0.0f)
			it = popups.erase(it);   // 动态内存物理擦除，规避堆栈溢出
		else
			++it;
	}
}

/**
 * @brief 悬浮事件弹窗图层多级垂直动态重叠层渲染与空间计算布局
 */
void GUI::drawPopups() {
	if (popups.empty()) return;
	settextstyle(18, 0, _T("微软雅黑"));
	setbkmode(TRANSPARENT);

	int baseY = 100; // 初始悬浮渲染 Y 轴视口起始线
	for (const auto& p : popups) {
		// 计算单行光栅化文本物理宽度，并横向扩展 40 像素作为舒适的 UI 呼吸边距
		int textWidth = textwidth(_T(p.text.c_str())) + 40;
		int textHeight = 35;
		int left = (WIN_WIDTH - textWidth) / 2; // 主屏幕视口水平居中算法
		int top = baseY;

		setfillcolor(RGB(255, 255, 220)); // 高视见高对比温暖底框配置
		setlinecolor(RGB(200, 200, 100));
		fillrectangle(left, top, left + textWidth, top + textHeight);

		settextcolor(RGB(180, 60, 60)); // 强对比警示文本染色
		outtextxy(left + 20, top + 8, _T(p.text.c_str()));

		baseY += textHeight + 10; // 线性垂直层叠拓扑步进，防止多重并发事件在视口重叠失真
	}
}

/**
 * @brief 右侧业务控制台组件：“电商前台自动接单状态开关”双稳态按钮图层及配色动态反馈渲染
 */
void GUI::drawWmsToggleButton() {
	int panelX = MAP_LENGTH * GRID_SIZE + 20; // 规避视口坐标错位的横向锚定基准
	int btnWidth = 240;
	int btnHeight = 40;
	int btnY = 300; // 优化垂直拓扑，实现与右侧调度操控台的一体化无缝对接

	wmsToggleButtonRect.left = panelX;
	wmsToggleButtonRect.top = btnY;
	wmsToggleButtonRect.right = panelX + btnWidth;
	wmsToggleButtonRect.bottom = btnY + btnHeight;

	// 多模态有限状态机分支变色逻辑：根据自动接单旗帜变量的真假，实时重构图形管线渲染底盘色彩
	if (isWmsAutoReceiving) {
		setfillcolor(RGB(46, 204, 113));  // 高饱和度高能生态绿：代表上游订单流持续吞吐
		setlinecolor(RGB(39, 174, 96));
	}
	else {
		setfillcolor(RGB(231, 76, 60));   // 强感知高警示高亮红：代表上游注入链路强制切断拦截
		setlinecolor(RGB(192, 57, 43));
	}

	// 驱动底层二维原子级带边框矩形包围盒填充
	fillrectangle(wmsToggleButtonRect.left, wmsToggleButtonRect.top,
		wmsToggleButtonRect.right, wmsToggleButtonRect.bottom);

	settextcolor(WHITE);
	settextstyle(18, 0, _T("微软雅黑"));
	setbkmode(TRANSPARENT);

	// 基于当前双稳态系统状态进行动态文本流重写
	std::string btnText = isWmsAutoReceiving ? "电商前台接单：运行中 (点击关闭)" : "电商前台接单：已暂停 (点击开启)";

	int textWidth = textwidth(_T(btnText.c_str()));
	int textHeight = textheight(_T(btnText.c_str()));
	int textX = wmsToggleButtonRect.left + (btnWidth - textWidth) / 2;
	int textY = wmsToggleButtonRect.top + (btnHeight - textHeight) / 2;

	outtextxy(textX, textY, _T(btnText.c_str()));
}