/*视觉交互*/
#pragma once
#include <graphics.h>
/*GUI接口*/
#include "common.h"
#include "WarehouseManager.h"

class GUI {
private:
    int screenLength;        //屏幕长度
    int screenWideth;       //屏幕宽度

public:
    GUI(int l, int w);          //构造函数，初始化屏幕尺寸
    ~GUI();                     //析构函数，清理资源

    // 渲染整个系统：先画图，再画 UI 文本，最后画机器人
    void render(const WarehouseManager& manager);

private:
    void drawGrid();        //画网格线
	void drawStatusPanel(const std::vector<Robot>& robots);     //画状态面板，显示每个机器人的状态
};