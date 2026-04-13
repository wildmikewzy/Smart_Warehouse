/*机器人的逻辑执行*/
#pragma once
#include "common.h"

class Robot {
public:
    int id;     //机器人的id号
    Point currentPos;       //机器人现在的点位
    std::vector<Point> pathQueue; // 存放 Router 给的路径
    RobotStatus status;     //机器人的状态
    std::vector<Point> historyPath;     //用于存储走过的历史轨迹点；
    Robot(int _id, Point start); //构造函数，初始化机器人的状态和位置
    void update(); // 每一帧调用一次，处理移动逻辑
    void setPath(const std::vector<Point>& newPath);        //接收 Router 给的路径，更新状态
private:
    float realX;
    float realY;
};