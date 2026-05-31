#include <windows.h>
#include "WarehouseManager.h"
#include "GUI.h"
#include "common.h"

int main() {
    // 实例化仓库控制中枢 (内部自动完成所有站点、货架和 4 辆 AGV 的初始化)
    WarehouseManager manager;

    // 实例化图形界面
    GUI gui(WIN_WIDTH, WIN_HEIGHT);

    // 帧率与时序控制初始化
    const DWORD LOGIC_INTERVAL_MS = 35; // 控制物理步进间隔（约相当于 ~30 FPS 的逻辑心跳）
    DWORD lastLogicTime = GetTickCount();
    DWORD lastFrameTime = GetTickCount();

    // 主仿真循环 (游戏级高频 Tick 驱动)
    // 增加 GetAsyncKeyState(VK_ESCAPE) 监听，如果按下 ESC 则退出循环
    while (!gui.isTimeout() && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        DWORD currentTime = GetTickCount();
        
        // 计算两帧之间的 deltaTime
        DWORD deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // 【第一步】：捕获交互（地图点击选车、侧边栏按钮派发业务等）
        gui.handleMouseClick(manager);

        // 【第二步】：业务推进（定时心跳更新）
        // 累积时间达到设定步进阈值时，触发一次状态机与物理位置的前进
        if (currentTime - lastLogicTime >= LOGIC_INTERVAL_MS) {
            manager.updateAll(); 
            lastLogicTime = currentTime;
        }

        // 【第三步】：弹窗更新（UI动效层实时跟进）
        gui.updatePopups(deltaTime);

        // 【第四步】：图形渲染
        gui.render(manager);

        // 【第五步】：休眠控频，降低 CPU 负载
        Sleep(10);
    }

    return 0; // 主循环结束后由 GUI 的析构函数平稳释放 EasyX / 窗口资源
}
