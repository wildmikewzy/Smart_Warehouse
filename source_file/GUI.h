/*视觉交互*/
#pragma once
#include <graphics.h>
/*GUI接口*/
#include "common.h"
#include "WarehouseManager.h"
#include <string>
//按钮结构体
class Button {
public:
	int x, y, width, height;        //按钮的坐标（左上方点）位置和相关尺寸
	bool isPressed;     //判断按钮是否被按下
	std::string text;	   //按钮上的文本
    bool contains(int mx, int my) const {       //判断鼠标坐标是否在按钮范围内
		return mx >= x && mx <= x + width && my >= y && my <= y + height;

    }
};
class GUI {
private:
    int screenLength;        //屏幕长度
    int screenWideth;       //屏幕宽度

    //两个控制按钮
    Button btn1;
    Button btn2;
public:
    GUI(int l, int w);          //构造函数，初始化屏幕尺寸
    ~GUI();                     //析构函数，清理资源

    // 渲染整个系统：先画图，再画 UI 文本，最后画机器人
    void render(const WarehouseManager& manager);

    // 处理鼠标点击事件，打印网格坐标到控制台
    void handleMouseClick(WarehouseManager& manager);

private:
    void drawGrid();        //画网格线
    void drawStatusPanel(const std::vector<Robot>& robots);     //画状态面板，显示每个机器人的状态
	void drawButtons();      //画按钮
};