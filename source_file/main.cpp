#include "GUI.h"
#include "WarehouseManager.h"
#include <iostream>
#include <windows.h>   // 调用GetAsyncKeyState，Sleep

using namespace std;
int main() {
    cout << "========================================" << endl;
    cout << "   Smart_Warehouse 项目启动测试程序" << endl;
    cout << "========================================" << endl;

	WarehouseManager manager;       // 创建仓库管理器实例
	GUI gui(WIN_WIDTH, WIN_HEIGHT);     // 创建 GUI 实例，并传入窗口尺寸

    bool isRunning = true;

    // 防重复触发标志（按下时置 true，松开后重置）
    bool keyNPressed = false;
    bool keyMPressed = false;
    bool keyESCPressed = false;

    while (isRunning) {
		// 检测 ESC 键，按下ESC键则安全退出程序
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            if (!keyESCPressed) {
                keyESCPressed = true;
                isRunning = false;
            }
        }
        else {
            keyESCPressed = false;
        }
        // 更新业务状态
        manager.updateAll();
        // 渲染画面
        BeginBatchDraw();
        gui.handleMouseClick(manager);
        gui.render(manager);
        EndBatchDraw();
		Sleep(30);      // 控制帧率，30ms，大约33帧
    }

    cout << "程序已安全退出。" << endl;
    return 0;
}