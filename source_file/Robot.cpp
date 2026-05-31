/*机器人动态逻辑接口*/
#include "Robot.h"
#include <algorithm> // 包含 std::min / std::max

// 静态成员初始化
std::vector<Robot*>* Robot::allRobots = nullptr;

void Robot::setRobotList(std::vector<Robot*>* list) {
    allRobots = list;
}

Robot::Robot(int _id, Point start)
    : id(_id), currentPos(start), status(RobotStatus::IDLE),
    currentSpeed(0.0f), moveCooldown(0), currentStepFrames(15) {

    // 初始化视觉坐标，让其和出生点重合
    realX = static_cast<float>(start.x);
    realY = static_cast<float>(start.y);
    // 记录初始待命点为 homePos
    homePos = start;

    // 初始化上一个位置
    lastPos = start;
    moveProgress = 0.0f;
    cellStepCompleted = false;
}

void Robot::setPath(const std::vector<Point>& newPath) {
    pathQueue = newPath;
    // 只要有新路径派发，重置进度条与起点基准
    moveProgress = 0.0f;
    lastPos = currentPos;
    cellStepCompleted = false;
    if (!pathQueue.empty()) {
        currentStepFrames = 1;
        moveCooldown = 0;  // 立即开始
    }
}

void Robot::update() {
    cellStepCompleted = false; // 每帧开始前重置单格跨越完成信号

    // ====================== 1. 业务装卸货/等待冷却处理 ======================
    if (status == RobotStatus::LOADING || status == RobotStatus::UNLOADING) {
        if (workCooldown > 0) {
            workCooldown--;
        }
        // 在装卸货期间，强行将视觉位置锁定在当前逻辑格
        realX = static_cast<float>(currentPos.x);
        realY = static_cast<float>(currentPos.y);
        lastPos = currentPos;
        moveProgress = 0.0f;
        currentSpeed = 0.0f;
        return;
    }

    // ====================== 2. 纯网格逻辑与路径处理 ======================
    if (pathQueue.empty()) {
        currentSpeed = 0.0f;
        // 没路径时，视觉位置必须精准等于逻辑位置
        realX = static_cast<float>(currentPos.x);
        realY = static_cast<float>(currentPos.y);
        lastPos = currentPos;
        moveProgress = 0.0f;
        return;
    }

    if (moveCooldown > 0) {
        moveCooldown--;
        return;
    }

    // 目标格子就是路径队列的第一个点
    Point target = pathQueue.front();

    // 如果目标点就是自己当前所在的点（寻路冗余保护），直接弹出
    if (target == currentPos) {
        pathQueue.erase(pathQueue.begin());
        lastPos = currentPos;
        moveProgress = 0.0f;
        return;
    }

    // ====================== 3. 匀速平滑插值核心 =/*匀速推进插值*/=====================
    // 设定每帧前进的进度百分比（0.1f 代表 10 帧走完一格，你可以通过调整这个值来改变车速）
    float speedStep = 0.1f;
    moveProgress += speedStep;
    currentSpeed = 20.0f; // 看板速度显示

    if (moveProgress >= 1.0f) {
        // 当进度条充满，说明小车这一个格子已经在视觉上完全走完
        moveProgress = 1.0f;

        // 真正的物理坐标在一帧内跨越到新格子
        currentPos = target;
        lastPos = currentPos; // 滚动起点基准
        moveProgress = 0.0f;  // 重置进度条

        // 写入历史轨迹
        if (historyPath.empty() || !(historyPath.back() == currentPos)) {
            historyPath.push_back(currentPos);
        }

        // 从路径队列中弹出已经走完的这一格
        pathQueue.erase(pathQueue.begin());

        // 【最核心触发机制】：通知外部管理器，这辆车刚刚跨越完一整格
        cellStepCompleted = true;

        if (pathQueue.empty()) {
            currentSpeed = 0.0f;
        }
    }

    // 根据上一格起点 lastPos 与目标格 target 严格按照分量直线插值，拒绝大斜线和鬼影！
    realX = static_cast<float>(lastPos.x) + (static_cast<float>(target.x) - static_cast<float>(lastPos.x)) * moveProgress;
    realY = static_cast<float>(lastPos.y) + (static_cast<float>(target.y) - static_cast<float>(lastPos.y)) * moveProgress;
}