/*机器人动态逻辑接口*/
#include "Robot.h"

Robot::Robot(int _id, Point start)          //Robot对象的构造函数，接受一个ID和起始位置，并初始化状态为IDLE（空闲）
    : id(_id), currentPos(start), status(RobotStatus::IDLE) {
    realX = start.x;
    realY = start.y;
}   //构造函数，初始化机器人的状态和位置

void Robot::setPath(const std::vector<Point>& newPath) {      // 接收 Router 给的路径，更新机器人的状态为 MOVING（移动中），并将路径存入 pathQueue
    pathQueue = newPath;
    if (!pathQueue.empty()) status = RobotStatus::MOVING;
}

void Robot::update() {      //移动更新处理
    if (status == RobotStatus::MOVING && !pathQueue.empty()) {
        Point target = pathQueue.front();

        // 计算当前位置与目标格点像素中心点的偏移，开始移动（先X后Y)
        float step = 0.1f;//移动长度

        if (abs(realX - target.x) > 0.01f) {
            if (realX < target.x) realX += step;
            else if (realX > target.x) realX -= step;
        }

        else if (abs(realY - target.y) > 0.01f) {
            if (realY < target.y) realY += step;
            else if (realY > target.y) realY -= step;
        }

		currentPos.x = float(round(realX));        // 将当前实际坐标四舍五入到最近的格点坐标，更新 currentPos
        currentPos.y = float(round(realY));        

        // 判定是否“足够接近”目标点（防止浮点数抖动）
        if (abs(realX - target.x) < 0.12f && abs(realY - target.y) < 0.12f) {
            realX = target.x;
            realY = target.y;
            currentPos = target;    // 强行对齐到格点
            pathQueue.erase(pathQueue.begin()); // 前往路径序列的下一个点
        }
        //记录轨迹：
        if (historyPath.empty() || !(historyPath.back() == currentPos)) {
            historyPath.push_back(currentPos);
        }
        // 判定到达终点后，机器人的状态变为空闲
        if (pathQueue.empty()) status = RobotStatus::IDLE;
    }
}
