#include <windows.h>
#include "WarehouseManager.h"
#include "GUI.h"
#include "common.h"

int main() {

	// 实例化仓库控制中枢 (内部自动完成所有站点、货架、4 辆 AGV 的初始化，并立发 4 单)
	WarehouseManager manager;

	// 实例化图形界面
	GUI gui(WIN_WIDTH, WIN_HEIGHT);

	// 帧率与时序控制初始化
	const DWORD LOGIC_INTERVAL_MS = 30; // 逻辑层更新频率（约 30ms 步进一次）
	DWORD lastLogicTime = GetTickCount();
	DWORD lastFrameTime = GetTickCount();

	// 主仿真循环 
	while (!gui.isTimeout() && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
		DWORD currentTime = GetTickCount();

		// 计算两帧之间的 deltaTime，用于 GUI 弹窗等实时动效的平滑度量
		DWORD deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;

		// 【第一步】：捕获交互
		gui.handleMouseClick(manager);

		// 【第二步】：业务推进（定时心跳更新）
		if (currentTime - lastLogicTime >= LOGIC_INTERVAL_MS) {
			manager.updateAll(gui);
			lastLogicTime = currentTime;
		}

		// 【第三步】：弹窗更新（UI动效层实时跟进）
		gui.updatePopups(deltaTime);

		// 【第四步】：图形渲染（以极高帧率疯狂渲染，绝不由于 Sleep 导致颜色变换发生卡顿）
		gui.render(manager);

		// 【第五步】
		Sleep(30);
	}
	if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
		system("pause");	
	}

	return 0;
}