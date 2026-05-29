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
    bool keyTPressed = false;

    int stressStage = 0;
    bool stressTest2Waiting = false;
    int stressTest2DelayFrames = 0;
    int stressTest2StationId = -1;

    auto getStationIdOrLog = [&](int x, int y, const string& label) -> int {
        int stationId = manager.getStationIdByGrid(x, y);
        if (stationId == -1) {
            cout << "[StressTest] " << label << "站点坐标(" << x << "," << y << ")无效，测试已跳过。" << endl;
        }
        return stationId;
    };

    auto triggerStressStage = [&](int stage) {
        if (stage == 1) {
            cout << "[StressTest] 阶段1：对头死锁冲突测试" << endl;

            int leftStationId = getStationIdOrLog(2, 2, "左侧");
            int rightStationId = getStationIdOrLog(16, 2, "右侧");
            if (leftStationId == -1 || rightStationId == -1) {
                return;
            }

            manager.prepareStressTest({ {1, {0, 10}}, {2, {18, 10}} });
            manager.dispatchRobot(1, rightStationId);
            manager.dispatchRobot(2, leftStationId);
        }
        else if (stage == 2) {
            cout << "[StressTest] 阶段2：终点抢占与静止占领测试" << endl;

            int targetStationId = getStationIdOrLog(2, 12, "共享终点");
            if (targetStationId == -1) {
                return;
            }

            manager.prepareStressTest({ {1, {0, 10}}, {2, {0, 9}} });
            manager.dispatchRobot(1, targetStationId);

            stressTest2Waiting = true;
            stressTest2DelayFrames = 45;
            stressTest2StationId = targetStationId;
        }
        else if (stage == 3) {
            cout << "[StressTest] 阶段3：十字路口四向“鬼探头”冲突测试" << endl;

            int topStationId = getStationIdOrLog(2, 2, "上侧");
            int bottomStationId = getStationIdOrLog(2, 12, "下侧");
            int rightStationId = getStationIdOrLog(16, 2, "右侧");
            int leftStationId = getStationIdOrLog(16, 12, "左侧");
            if (topStationId == -1 || bottomStationId == -1 || rightStationId == -1 || leftStationId == -1) {
                return;
            }

            manager.prepareStressTest({ {1, {9, 8}}, {2, {9, 11}}, {3, {8, 9}}, {4, {11, 9}} });
            manager.dispatchRobot(1, bottomStationId);
            manager.dispatchRobot(2, topStationId);
            manager.dispatchRobot(3, rightStationId);
            manager.dispatchRobot(4, leftStationId);
        }
        else if (stage == 4) {
            cout << "[StressTest] 阶段4：动态尾随与追尾测试" << endl;

            int rightStationId = getStationIdOrLog(16, 2, "右侧");
            if (rightStationId == -1) {
                return;
            }

            manager.prepareStressTest({ {1, {1, 10}}, {2, {0, 10}} });
            manager.dispatchRobot(1, rightStationId);
            manager.dispatchRobot(2, rightStationId);
        }
    };

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

        if (GetAsyncKeyState('T') & 0x8000) {
            if (!keyTPressed) {
                keyTPressed = true;
                stressStage = (stressStage % 4) + 1;
                triggerStressStage(stressStage);
            }
        }
        else {
            keyTPressed = false;
        }

        if (stressTest2Waiting) {
            if (stressTest2DelayFrames > 0) {
                stressTest2DelayFrames--;
            }
            if (stressTest2DelayFrames == 0) {
                cout << "[StressTest] 终点抢占测试：派发2号机器人" << endl;
                manager.dispatchRobot(2, stressTest2StationId);
                stressTest2Waiting = false;
            }
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