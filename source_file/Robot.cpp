/*机器人动态逻辑接口*/
#include "Robot.h"
#include <cmath>

// 静态成员初始化
std::vector<Robot*>* Robot::allRobots = nullptr;

void Robot::setRobotList(std::vector<Robot*>* list) {
    allRobots = list;
}

Robot::Robot(int _id, Point start)
    : id(_id), currentPos(start), status(RobotStatus::IDLE),
    currentSpeed(0.0f) {
    realX = start.x;
    realY = start.y;
}

void Robot::setPath(const std::vector<Point>& newPath) {
    pathQueue = newPath;
    if (!pathQueue.empty()) {
        status = RobotStatus::MOVING;
        currentSpeed = 0.0f;  // 新路径速度归零
    }
}

void Robot::update() {
    // 不在移动或无路径 → 停止
    if (status != RobotStatus::MOVING || pathQueue.empty()) {
        currentSpeed = 0.0f;
        return;
    }

    Point target = pathQueue.front();

    // 当前位置到目标点的向量与距离
    float dx = target.x - realX;
    float dy = target.y - realY;
    float dist = std::sqrt(dx * dx + dy * dy);

    // ====================== 1. 剩余总距离计算 ======================
    float totalRemainingDist = dist;
    for (size_t i = 0; i < pathQueue.size() - 1; ++i) {
        const Point& p1 = pathQueue[i];
        const Point& p2 = pathQueue[i + 1];
        float d = std::hypot(p2.x - p1.x, p2.y - p1.y);
        totalRemainingDist += d;
    }

    // ====================== 2. 前方机器人碰撞检测 ======================
    const float SAFE_DIST = 1.2f;
    bool blocked = false;

    if (allRobots != nullptr) {
        for (Robot* other : *allRobots) {
            if (other->id == id) continue;

            float odx = other->realX - realX;
            float ody = other->realY - realY;
            float robotDist = std::sqrt(odx * odx + ody * ody);

            // 距离过近 + 在前进方向上 → 视为阻挡
            if (robotDist < SAFE_DIST && robotDist > 0.001f) {
                float dotProduct = (dx * odx + dy * ody) / robotDist;
                if (dotProduct > 0.0f) {
                    blocked = true;
                    break;
                }
            }
        }
    }

    // ====================== 3. 梯形速度曲线 ======================
    const float MAX_SPEED = 2.0f;
    const float ACCEL = 0.03f;//初始加速度
    const float DECEL = 0.03f;//减速加速度
    float targetSpeed = 0.0f;

    if (!blocked) {
        // 刹车距离公式：v²/(2a) → 提前减速防止冲过终点
        float brakeDist = (currentSpeed * currentSpeed) / (2.0f * DECEL);
        if (totalRemainingDist < brakeDist) {
            // 渐进减速
            targetSpeed = std::sqrt(2.0f * DECEL * totalRemainingDist);
        }
        else {
            targetSpeed = MAX_SPEED;
        }
    }

    // ====================== 4. 平滑加减速 ======================
    if (currentSpeed < targetSpeed) {
        currentSpeed = std::min(currentSpeed + ACCEL, targetSpeed);
    }
    else {
        currentSpeed = std::max(currentSpeed - DECEL, targetSpeed);
    }

    // 极小速度直接归零，避免抖动
    if (currentSpeed < 0.01f) {
        currentSpeed = 0.0f;
    }

    // ====================== 5. 执行移动 ======================
    float step = currentSpeed;
    if (step > dist) {
        step = dist;  // 防止跨过目标点
    }

    if (dist > 0.001f) {
        realX += (dx / dist) * step;
        realY += (dy / dist) * step;
    }

    // 视觉坐标使用浮点，不跳变（要显示整数再自己取整）
    currentPos.x = realX;
    currentPos.y = realY;

    // ====================== 6. 到达路点判断 ======================
    const float ARRIVE_THRESHOLD = 0.15f;  // 放宽一点更稳定
    if (dist < ARRIVE_THRESHOLD) {
        // 强制贴到目标点，避免误差累积
        realX = target.x;
        realY = target.y;
        currentPos = target;

        if (!pathQueue.empty()) {
            pathQueue.erase(pathQueue.begin());
        }
    }
    // ====================== 7. 记录历史路径（去重） ======================
    if (historyPath.empty() || !(historyPath.back() == currentPos)) {
        historyPath.push_back(currentPos);
    }

    // 路径走完 →  idle
    if (pathQueue.empty()) {
        status = RobotStatus::IDLE;
        currentSpeed = 0.0f;
    }
}