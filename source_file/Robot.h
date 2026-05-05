/*机器人的逻辑执行*/
#pragma once
#include "common.h"
#include <vector>
// 机器人类
class Robot {
public:
    // 构造函数
    Robot(int _id, Point startPos);

    // 路径与状态
    void setPath(const std::vector<Point>& newPath);
    void update();  // 每帧调用

    // 静态机器人列表
    static std::vector<Robot*>* allRobots;
    static void setRobotList(std::vector<Robot*>* list);

    // 公开成员（只读访问建议改成 getter，这里保持原有风格）
    int id;
    Point currentPos;
    RobotStatus status;
    float currentSpeed;

    // 路径数据
    std::vector<Point> pathQueue;
    std::vector<Point> historyPath;

private:
    // 真实浮点坐标
    float realX;
    float realY;
};