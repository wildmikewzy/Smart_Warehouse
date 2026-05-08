#include "GUI.h"
#include "WarehouseManager.h"
#include <iostream>
#include <windows.h>   // GetAsyncKeyState

using namespace std;

int main() {
    cout << "========================================" << endl;
    cout << "   Smart_Warehouse 项目启动测试程序" << endl;
    cout << "========================================" << endl;
    cout << "操作说明：" << endl;
    cout << "  按键 N : 派发机器人1前往 (6,9)" << endl;
    cout << "  按键 M : 派发机器人2前往 (8,9)" << endl;
    cout << "  按 ESC : 退出系统" << endl;
    cout << "----------------------------------------" << endl;

    WarehouseManager manager;
    GUI gui(WIN_WIDTH, WIN_HEIGHT);

    bool isRunning = true;

    // 防重复触发标志（按下时置 true，松开后重置）
    bool keyNPressed = false;
    bool keyMPressed = false;
    bool keyESCPressed = false;

    while (isRunning) {
        // ---------- 使用 GetAsyncKeyState 代替 _kbhit/_getch ----------
        // 检测 N 键
        if (GetAsyncKeyState('N') & 0x8000) {
            if (!keyNPressed) {
                keyNPressed = true;
                manager.dispatchRobot(1, { 6, 9 });
                gui.addPopup("已派发机器人1前往 (6,9)");
            }
        }
        else {
            keyNPressed = false;
        }

        // 检测 M 键
        if (GetAsyncKeyState('M') & 0x8000) {
            if (!keyMPressed) {
                keyMPressed = true;
                manager.dispatchRobot(2, { 8, 9 });
                gui.addPopup("已派发机器人2前往 (8,9)");
            }
        }
        else {
            keyMPressed = false;
        }

        // 检测 ESC 键
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            if (!keyESCPressed) {
                keyESCPressed = true;
                isRunning = false;
            }
        }
        else {
            keyESCPressed = false;
        }
        // ------------------------------------------------------------

        // 更新业务状态
        manager.updateAll();

        // 渲染画面
        BeginBatchDraw();
        gui.handleMouseClick(manager);
        gui.render(manager);
        EndBatchDraw();

        Sleep(30);
    }

    cout << "程序已安全退出。" << endl;
    return 0;
}