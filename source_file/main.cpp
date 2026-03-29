#include "GUI.h"
#include "WarehouseManager.h"
#include <conio.h>
#include <iostream>

using namespace std;

int main() {
    /*架构测试*/

    // 1. 打印欢迎信息到控制台（已经开启了 EX_SHOWCONSOLE，在创建图形化窗口的时候保留控制台）
    cout << "========================================" << endl;
    cout << "   Smart_Warehouse 项目启动测试程序" << endl;
    cout << "========================================" << endl;
    cout << "操作说明：" << endl;
    cout << "  [N] : 发送测试订单 (机器人1号前往 14,9)" << endl;
    cout << "  [M] : 发送测试订单 (机器人2号前往 13,9)" << endl;
    cout << "  [ESC] : 退出系统" << endl;
    cout << "----------------------------------------" << endl;

    // 2. 初始化业务管理器（内部会自动调用 setupScene）
    WarehouseManager manager;

    // 3. 初始化 GUI 窗口
    // 宽度 = 地图长*像素大小 + 侧边栏；高度 = 地图宽*像素大小
    GUI gui(WIN_WIDTH, WIN_HEIGHT);

    bool isRunning = true;

    // 4. 主循环
    while (isRunning) {
        // --- 逻辑阶段 A: 处理输入 ---
        if (_kbhit()) {     //_kbhit() 函数用于检查控制台窗口是否有按键被按下
            char key = _getch();       // _getch() 函数是一个用于从控制台读取单个字符的函数，但它不会在屏幕上显示该字符。
            if (key == 'n' || key == 'N') {
                cout << "[指令] 机器人 1 号前往货架区 (14, 9)" << endl;
                manager.dispatchRobot(1, { 14, 9 });
            }
            else if (key == 'm' || key == 'M') {
                cout << "[指令] 机器人 2 号前往出货点 (13, 9)" << endl;
                manager.dispatchRobot(2, { 13, 9 });
            }
            else if (key == 27) { // ESC 键
                isRunning = false;
            }
        }

        // --- 逻辑阶段 B: 更新状态 ---
        manager.updateAll();

        // --- 逻辑阶段 C: 渲染画面 ---
        BeginBatchDraw(); // 开启批量绘图，杜绝闪烁

        gui.render(manager);

        EndBatchDraw(); // 结束并显示

        // 适当休眠，降低 CPU 占用并控制动画速度
        Sleep(30);
    }

    cout << "程序已安全退出。" << endl;
    return 0;
}