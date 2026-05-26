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
}

void Robot::setPath(const std::vector<Point>& newPath) {
    pathQueue = newPath;
    if (!pathQueue.empty()) {
        status = RobotStatus::MOVING;
        // 起步时最慢，走一格需要 15 帧
        currentStepFrames = 15;
        moveCooldown = 0;  // 立即开始第一步
    }
}

void Robot::update() {
    // === 永远执行的视觉平滑过滤部分 ===
    // 不论逻辑有没有到达下一格，视觉上始终以追赶的方式向 currentPos(逻辑格) 滑动
    float dx_vis = static_cast<float>(currentPos.x) - realX;
    float dy_vis = static_cast<float>(currentPos.y) - realY;
    
    // 平滑追赶缓冲比例（例如每帧靠近差距的25%）
    // 数值越小越“拖尾滑行”，数值越大越紧贴网格
    realX += dx_vis * 0.25f;
    realY += dy_vis * 0.25f;

    // === 以下为纯网格逻辑处理 ===
    if (status != RobotStatus::MOVING || pathQueue.empty()) {
        currentSpeed = 0.0f;
        return;
    }

    // 处理当前格子的冷却（停留时间）
    if (moveCooldown > 0) {
        moveCooldown--;
        return; // 还没冷却好，这一帧继续停留在此格
    }

    Point target = pathQueue.front();

    // ====================== 1. 离散锁格防碰撞检测 ======================
    bool blocked = false;
    if (allRobots != nullptr) {
        for (Robot* other : *allRobots) {
            if (other->id == id) continue;
            // 简单判定：如果有其他机器人已经在我们的目标格，就卡住
            if (other->currentPos == target) {
                blocked = true;
                break;
            }
        }
    }

    if (blocked) {
        // 如果被挡住了，重头慢速起步
        currentStepFrames = 15; 
        moveCooldown = 0; // 不刷新长冷却，下一帧继续检测是否通畅
        currentSpeed = 0.0f; // UI 速度显示为 0
        return; // 即使被挡住不移动，最上方的视觉 realX 也会追赶贴合到网格中心
    }

    // ====================== 2. 执行移动 (逻辑瞬间进驻新网格) ======================
    currentPos = target;

    // ====================== 3. 计算离散加减速 (调整下一次的冷却帧数) ======================
    int remainingSteps = static_cast<int>(pathQueue.size());
    
    // 基础定义：帧数越少，车开得越快
    const int MAX_SPEED_FRAMES = 3;  // 最高速：只需 3 帧换一格
    const int MIN_SPEED_FRAMES = 15; // 最慢速：需 15 帧换一格
    
    // 剩余步数小于 4 步时，开始逐渐增加所需的帧数（减速）
    if (remainingSteps <= 4) {
        currentStepFrames += 3; // 猛踩刹车：每走一步，下次多停3帧
    } else {
        // 否则持续减少所需的帧数（加速），直到到达满速
        currentStepFrames -= 2; 
    }
    
    // 限幅控制
    currentStepFrames = std::max(MAX_SPEED_FRAMES, std::min(currentStepFrames, MIN_SPEED_FRAMES));
    
    // 为了 GUI 状态面板看起来合理，将帧数倒数映射为表观数字速度
    currentSpeed = (float)MIN_SPEED_FRAMES / currentStepFrames;

    // 重置冷却器，准备下一次逻辑跳跃
    moveCooldown = currentStepFrames;

    // ====================== 4. 后处理 ======================
    // 记录历史路径
    if (historyPath.empty() || !(historyPath.back() == currentPos)) {
        historyPath.push_back(currentPos);
    }

    // 抹除刚才已走过的节点
    pathQueue.erase(pathQueue.begin());

    if (pathQueue.empty()) {
        status = RobotStatus::IDLE;
        currentSpeed = 0.0f;
    }
}