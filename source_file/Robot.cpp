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
    // 记录初始待命点为
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
/**
* @brief Robot 的核心更新函数，负责处理移动、装卸货逻辑以及视觉坐标的平滑插值（全面免疫死锁优化版）
*/
void Robot::update() {
    cellStepCompleted = false; // 每帧开始前重置单格跨越完成信号

    if (allRobots == nullptr) return;

    // ====================== 1. 业务装卸货/等待冷却处理 ======================
    if (status == RobotStatus::LOADING || status == RobotStatus::UNLOADING) {
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

    bool visualBlocked = false;
    Point nextPos = pathQueue.front(); // 我下一步要去的格子

    // 【针对直道递补与回巢的雷达免检芯片】：
    // 如果我本身正在排队直道执行强力递补，或者我已经快到老家了，直接免检，跳过空气墙判定！
    bool amIExempt = false;
    if (status == RobotStatus::MOVING_TO_DELIVER && currentPos.y == 10) {
        amIExempt = true; // 正在排队道上递补的车，100% 安全，免检！
    }
    if (status == RobotStatus::RETURNING_BUFFER && nextPos.x == 0) {
        amIExempt = true; // 正在迈入老家出生点(X=0)的最后一步车，免检！
    }

    // ====================================================================
// 🌟【完美终结版】：破除帧内时序错位 + 彻底消除穿模硬核优化
// ====================================================================
    bool headOnDeadlockDetected = false;
    Robot* deadlockedPartner = nullptr;

    if (!amIExempt) {
        for (const auto& otherPtr : *allRobots) {
            if (otherPtr == nullptr) continue;
            if (otherPtr == this) continue;

            // 1. 检查对方当前站立的格子，是不是我正准备要去的格子
            if (otherPtr->currentPos == nextPos) {
                if (otherPtr->moveProgress < 1.0f) {

                    // 动态获取对方小车的下一步目标点
                    Point otherNextPos = otherPtr->pathQueue.empty() ? otherPtr->currentPos : otherPtr->pathQueue.front();

                    // 🚨【核心修正点】：对头死锁探测（完美兼容高ID车已提前清空路径的异步场景）
                    // 情况 A：对方的目标格子是我当前格子（正面对撞）
                    // 情况 B：对方路径已空（说明它是高ID车，已经在同一帧内抢先执行了退让，把路径轰碎了）
                    if (otherNextPos == this->currentPos || otherPtr->pathQueue.empty()) {
                        headOnDeadlockDetected = true;
                        deadlockedPartner = otherPtr;
                        break; // 撕开循环，由下方统一处理 ID 破除对称性
                    }

                    // 2. 如果只是普通的同向追踪（前车屁股还没挪干净），正常维持视觉挂起
                    visualBlocked = true;
                    break;
                }
            }
        }
    }

    // 🌟【死锁自愈核心】：利用 ID 破除两车对称硬锁
    if (headOnDeadlockDetected && deadlockedPartner != nullptr) {
        if (this->id > deadlockedPartner->id) {
            // 逼大让小：我是高 ID 车，我无条件退让
            this->pathQueue.clear();          // 1. 轰碎自己后续的所有路径
            this->moveProgress = 0.0f;        // 2. 动画进度归零，退回己方格子中心
            this->realX = static_cast<float>(currentPos.x);
            this->realY = static_cast<float>(currentPos.y);

            // 3. 强行抛出格点完成信号，逼迫上游 WMS 在下一帧为我【重新寻路绕道】
            this->cellStepCompleted = true;
            return;                           // 闪退本帧，腾出物理和逻辑空间
        }
        else {
            // 🛠️【终极修复】：我是低 ID 车
            // 此时高 ID 车已经执行退让并闪回了它的格子中心（moveProgress=0）。
            // 如果我死死钉在原地（比如原来的 moveProgress=0.8），在画面上就会显得我直接“插”进了对方身体里。
            // 解决方案：低 ID 车在这一帧也必须把动画进度无条件归零，优雅地退回自己的格子中心！保持两车安全的视觉间距！
            this->moveProgress = 0.0f;
            this->realX = static_cast<float>(currentPos.x);
            this->realY = static_cast<float>(currentPos.y);
            visualBlocked = true;
        }
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

    // ====================== 3. 匀速平滑插值核心 =====================
    float speedStep = 0.1f;
    moveProgress += speedStep;
    currentSpeed = 20.0f; // 看板速度显示

    if (moveProgress >= 1.0f) {
        // 当进度条充满，说明小车这一个格子已经在视觉上完全走完
        moveProgress = 1.0f;
        this->recordGridMovement();
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

    // 根据上一格起点 lastPos 与目标格 target 严格按照分量直线插值
    realX = static_cast<float>(lastPos.x) + (static_cast<float>(target.x) - static_cast<float>(lastPos.x)) * moveProgress;
    realY = static_cast<float>(lastPos.y) + (static_cast<float>(target.y) - static_cast<float>(lastPos.y)) * moveProgress;
}
void Robot::recordGridMovement() {
    // 只有在实际发生网格位移的状态下才计数
    if (status == RobotStatus::MOVING_TO_PICK || status == RobotStatus::RETURNING_BUFFER) {
        totalGridsWalked++;
        emptyGridsWalked++;
    }
    else if (status == RobotStatus::MOVING_TO_DELIVER) {
        totalGridsWalked++;
    }
    }