/*机器人动态逻辑接口*/
#include "Robot.h"

Robot::Robot(int _id, Point start) : id(_id), currentPos(start), status(RobotStatus::IDLE) {}   //构造函数，初始化机器人的状态和位置

void Robot::setPath(const std::vector<Point>& newPath) {            // 接收 Router 给的路径，更新状态
    pathQueue = newPath;
    if (!pathQueue.empty()) status = RobotStatus::MOVING;
}

void Robot::update() {          //每帧调用一次，处理移动逻辑
    if (status == RobotStatus::MOVING && !pathQueue.empty()) {
        // 简单的逻辑：每调用一次，取路径中的下一个点
        // 任务：在这里需要实现平滑移动动画，而不是瞬间跳格
        currentPos = pathQueue.front();
        pathQueue.erase(pathQueue.begin());

        if (pathQueue.empty()) status = RobotStatus::IDLE;
    }
}