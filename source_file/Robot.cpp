/*机器人动态逻辑接口*/
#include "Robot.h"
#include <algorithm> // 包含 std::min / std::max

// 全局静态成员变量初始化：用于多智能体帧内时序穿透与位置互检
std::vector<Robot*>* Robot::allRobots = nullptr;

/**
 * @brief 配置全局智能体动态监测指针大盘
 * @param list 包含系统中所有在线智能体指针的动态关联数组
 * @note 允许各智能体在单机更新过程中执行帧内穿透，进行去中心化的邻域冲突特征检测。
 */
void Robot::setRobotList(std::vector<Robot*>* list) {
    allRobots = list;
}

/**
 * @brief 智能体类构造函数
 * @param _id 智能体全局唯一识别标识号
 * @param start 智能体在静态地图中的出生/物理初始格点坐标
 * @note 负责位姿状态、动态控制参数（运动冷却步、标称速度级别）及视觉平滑渲染基准（realX/Y）的物理初始化。
 */
Robot::Robot(int _id, Point start)
    : id(_id), currentPos(start), status(RobotStatus::IDLE),
    currentSpeed(0.0f), moveCooldown(0), currentStepFrames(15) {

    // 初始化三维视觉平滑插值坐标，使其在出生瞬间与逻辑网格中心点强行对齐
    realX = static_cast<float>(start.x);
    realY = static_cast<float>(start.y);

    // 记录标称初始待命点（Home Base），用于长期闲置状态下的自动回巢策略
    homePos = start;

    // 初始化时空位移滚动基准
    lastPos = start;
    moveProgress = 0.0f;
    cellStepCompleted = false;
}

/**
 * @brief 路由路径队列派发与时空状态重置接口
 * @param newPath 由上游中央调度系统解算并下发的多维无冲突时空拓扑路径向量
 * @note 当小车被赋予新作业任务或执行自愈重寻路时，该接口负责重置平滑运动插值进度条，并配置控制层触发状态。
 */
void Robot::setPath(const std::vector<Point>& newPath) {
    pathQueue = newPath;

    // 只要有新路径派发，立刻重置平滑过渡进度条与逻辑起点基准，确保轨迹连续性
    moveProgress = 0.0f;
    lastPos = currentPos;
    cellStepCompleted = false;

    if (!pathQueue.empty()) {
        currentStepFrames = 1;
        moveCooldown = 0;  // 动态清空物理冷却计步，激活即时驱动状态
    }
}

/**
 * @brief 智能体运动控制核心推推演与状态更新函数
 * @param table 全局时空资源预订快照表（Reservation Table）的引用
 * @param map 静态网络地图障碍物分布看板的常量引用
 * @param currentT 系统当前的绝对离散时间戳计数
 * @note 工业级控制核心。内含特定通道时空动态碰撞防护豁免机制、帧内非同步对头死锁检测算子、
 * 基于ID主键的冲突对称性消解机制（逼大让小自愈算法）以及双精确度视觉轨迹平滑线性插值算法。
 */
void Robot::update(ReservationTable& table, const Map& map, int currentT) {
    cellStepCompleted = false; // 每帧控制循环触发前，刚性重置单格跨越完成信号

    if (allRobots == nullptr) return;

    // ====================================================================
    // 1. 业务态作业行为（装载/卸载）锁定与控制拦截
    // ====================================================================
    if (status == RobotStatus::LOADING || status == RobotStatus::UNLOADING) {
        // 核心流控：在接驳执行装卸货动作期间，强行将视觉渲染物理坐标锁死在当前逻辑格点中心
        realX = static_cast<float>(currentPos.x);
        realY = static_cast<float>(currentPos.y);
        lastPos = currentPos;
        moveProgress = 0.0f;
        currentSpeed = 0.0f;
        return; // 拦截后续位移动力学推进
    }

    // ====================================================================
    // 2. 标称空路径状态下的位姿静态约束
    // ====================================================================
    if (pathQueue.empty()) {
        currentSpeed = 0.0f;
        // 核心约束：在无路由指令的闲置状态下，视觉物理渲染位置必须严格全等于底层拓扑逻辑位置
        realX = static_cast<float>(currentPos.x);
        realY = static_cast<float>(currentPos.y);
        lastPos = currentPos;
        moveProgress = 0.0f;
        return;
    }

    bool visualBlocked = false;
    Point nextPos = pathQueue.front(); // 提取当前状态转移方程中的前瞻目标拓扑格子

    // ====================================================================
    // 3. 特定通道时空动态碰撞防护豁免机制检测
    // ====================================================================
    // 业务背景：当智能体处于高密度单向递补主通道或正在迈入复位重置区时，其时空轨迹已通过大盘静态通道拓扑校验。
    // 为了极致优化高频循环中的开销，设立免检策略，直接跳过后续复杂的邻域多车空气墙判定。
    bool amIExempt = false;
    if (status == RobotStatus::MOVING_TO_DELIVER && currentPos.y == 10) {
        amIExempt = true; // 处于排队大干道（Y=10）执行强力位移递补的车辆，予以安全裁剪豁免
    }
    if (status == RobotStatus::RETURNING_BUFFER && nextPos.x == 0) {
        amIExempt = true; // 处于迈入初始缓存复位区（X=0）最终收尾阶段的智能体，予以安全裁剪豁免
    }

    // ====================================================================
    // 4. 帧内非同步对头死锁检测算子（破除异步时序错位与穿模问题）
    // ====================================================================
    bool headOnDeadlockDetected = false;
    Robot* deadlockedPartner = nullptr;

    if (!amIExempt) {
        for (const auto& otherPtr : *allRobots) {
            if (otherPtr == nullptr) continue;
            if (otherPtr == this) continue;

            // 步骤 4.1：前瞻碰撞判定。检测冲突对端当前所处的空间格点，是否与本车的下一步目标网格 nextPos 发生重叠
            if (otherPtr->currentPos == nextPos) {
                // 步骤 4.2：时空过渡状态检查。若对方尚未在物理上完全移出该网格（平滑进度未拉满）
                if (otherPtr->moveProgress < 1.0f) {

                    // 动态提取冲突对端的下一步期望拓扑目标点
                    Point otherNextPos = otherPtr->pathQueue.empty() ? otherPtr->currentPos : otherPtr->pathQueue.front();

                    // 🚨 相对死锁特征判定：
                    // 情况 A（正面对撞）：对方的下一步目标是本车当前格子。
                    // 情况 B（异步真空）：对方路由队列已空（说明对端是高 ID 智能体，已在同一帧的先序更新中抢先触发避让，擦除了自身路径）。
                    if (otherNextPos == this->currentPos || otherPtr->pathQueue.empty()) {
                        headOnDeadlockDetected = true;
                        deadlockedPartner = otherPtr;
                        break; // 撕开邻域遍历，转入下方统一的冲突对称性消解业务块
                    }

                    // 步骤 4.3：若不满足上述死锁特征，则判定为标称同向追踪状态（前车屁股释放过渡期），执行常规视觉挂起
                    visualBlocked = true;
                    break;
                }
            }
        }
    }

    // ====================================================================
    // 5. 冲突对称性消解机制（利用 ID 全局唯一性打破双车物理硬锁闭）
    // ====================================================================
    if (headOnDeadlockDetected && deadlockedPartner != nullptr) {

        // 步骤 5.1：两车同时执行制动级物理回弹，将平滑移动进度瞬时归零，安全回归到各自的逻辑网格中心点
        this->moveProgress = 0.0f;
        this->realX = static_cast<float>(currentPos.x);
        this->realY = static_cast<float>(currentPos.y);

        // 步骤 5.2：临终遗嘱保存。在清空本地队列前，紧急抓取并提取该路由原本关联的最终作业目的地
        Point finalDestination = this->pathQueue.back();

        // 步骤 5.3：时空表环境自愈。彻底擦除当前智能体在全局时空预订表中遗留的所有未来路径占位信息，杜绝幽灵死格
        for (size_t i = 0; i < pathQueue.size(); ++i) {
            int futureT = currentT + 1 + static_cast<int>(i);
            TimePoint oldPoint = { pathQueue[i].x, pathQueue[i].y, futureT };
            table.vertexReservations.erase(oldPoint);
        }

        // 步骤 5.4：切断本地旧路由羁绊，清空本地过时路由轨迹队列
        this->pathQueue.clear();

        // 步骤 5.5：通过非对称 ID 进行优先级行为分流（逼大让小策略）
        if (this->id > deadlockedPartner->id) {
            // ----------------------------------------------------------------
            // 策略 A：高标识度优先级策略（当前智能体执行即时重寻路与状态空间重构）
            // ----------------------------------------------------------------

            // 首先锁定自身当前屁股底下的拓扑格子，强制告知低 ID 智能体重规划时此格不可穿透
            TimePoint lockCurrent = { currentPos.x, currentPos.y, currentT };
            table.vertexReservations[lockCurrent] = this->id;

            // 调用核心 Router 接口重塑路由。传入当前位置与历史目标终点，时钟步长从下一时刻 (currentT + 1) 开始平滑切入
            std::vector<Point> newPath = Router::getPath(currentPos, finalDestination, map, table, this->id, currentT + 1);

            if (!newPath.empty()) {
                this->pathQueue = newPath;
                // 将新求解出来的避让路由重新追写进全局时空预订大盘
                for (size_t i = 0; i < newPath.size(); ++i) {
                    TimePoint newPoint = { newPath[i].x, newPath[i].y, currentT + 1 + static_cast<int>(i) };
                    table.vertexReservations[newPoint] = this->id;
                }
                visualBlocked = false; // 成功重构空间状态，解除挂起阻断
            }
            else {
                // 如果侧方逃逸干道被其他交通流锁死，导致无任何变道绕行空间，则被迫退化为原地强行多锁闭数帧未来时间窗
                for (int dt = 1; dt <= 3; ++dt) {
                    TimePoint futureLock = { currentPos.x, currentPos.y, currentT + dt };
                    table.vertexReservations[futureLock] = this->id;
                }
                visualBlocked = true; // 侧向空间资源耗尽，被迫维持挂起
            }

            this->cellStepCompleted = true; // 强行向外部任务层抛出状态变更信号
            return; // 闪退当前帧控制分支，释放计算上下文
        }
        else {
            // ----------------------------------------------------------------
            // 策略 B：低标识度阻断占位（当前智能体执行原地固守保护）
            // ----------------------------------------------------------------
            // 向未来高调拉满当前格子的专属所有权（强行锁定 5 个离散时间步），构建高强度未来阻断窗，迫使对向高 ID 车必须选择外围变道
            for (int dt = 0; dt < 5; ++dt) {
                TimePoint persistentLock = { currentPos.x, currentPos.y, currentT + dt };
                table.vertexReservations[persistentLock] = this->id;
            }

            // 低优先车辆同时同步尝试一次即时重寻路（探测是否能通过局部松弛接通原有目的）
            std::vector<Point> newPath = Router::getPath(currentPos, finalDestination, map, table, this->id, currentT + 1);
            if (!newPath.empty()) {
                this->pathQueue = newPath;
                for (size_t i = 0; i < newPath.size(); ++i) {
                    TimePoint newPoint = { newPath[i].x, newPath[i].y, currentT + 1 + static_cast<int>(i) };
                    table.vertexReservations[newPoint] = this->id;
                }
            }
            visualBlocked = true; // 本帧暂时维持驻留挂起状态
        }
    }

    // 步骤 5.6：常规步进冷却计数递减
    if (moveCooldown > 0) {
        moveCooldown--;
        return;
    }

    // 步骤 5.7：若由于常规追尾等环境因素触发视觉阻断，直接拦截，暂停推进物理步进进度条
    if (visualBlocked) {
        currentSpeed = 0.0f;
        return;
    }

    // 提取当前真正准备执行切入的目标网格点
    Point target = pathQueue.front();

    // 冗余防御性健壮保护：如果目标点就是自身当前站立的点，说明存在零位移时空等待，直接弹出路由队列首元素
    if (target == currentPos) {
        pathQueue.erase(pathQueue.begin());
        lastPos = currentPos;
        moveProgress = 0.0f;
        return;
    }

    // ====================================================================
    // 6. 双精确度时空匀速平滑插值算法核心
    // ====================================================================
    float speedStep = 0.1f; // 单物理帧步进的平滑分辨率增量
    moveProgress += speedStep;
    currentSpeed = 20.0f; // 更新 GUI 看板显示的实时运行速率

    if (moveProgress >= 1.0f) {
        // 核心临界点：当进度条饱和（>=1.0），宣告当前单格的空间物理位移在视觉渲染层彻底完成
        moveProgress = 1.0f;

        // 调用数据统计层指标记录接口，累加功耗与里程数
        this->recordGridMovement();

        // 核心原子操作：底层逻辑网格坐标正式跃迁切换至新目标格子中心
        currentPos = target;
        lastPos = currentPos; // 滚动刷新下一格平滑插值的空间起点基准
        moveProgress = 0.0f;  // 重置平滑进度计数器

        // 维护并写入历史行驶轨迹面包屑队列，用于前端复杂的热力图与长效尾迹渲染
        if (historyPath.empty() || !(historyPath.back() == currentPos)) {
            historyPath.push_back(currentPos);
        }

        // 弹出队列首节点，宣告该路由空间点的生命周期终结
        pathQueue.erase(pathQueue.begin());

        // 触发机制：向外部 WMS/WCS 统一管理器发布强力异步通知，表明本智能体刚刚完满跨越完一整格逻辑网格
        cellStepCompleted = true;

        if (pathQueue.empty()) {
            currentSpeed = 0.0f; // 路径收敛，速度平滑归零
        }
    }

    // 运动学方程：根据上一格逻辑起点 lastPos 与目标格 target，严格按照一阶分量直线执行视觉坐标实时插值
    realX = static_cast<float>(lastPos.x) + (static_cast<float>(target.x) - static_cast<float>(lastPos.x)) * moveProgress;
    realY = static_cast<float>(lastPos.y) + (static_cast<float>(target.y) - static_cast<float>(lastPos.y)) * moveProgress;
}

/**
 * @brief 智能体网格位移绩效与统计指标记录函数
 * @note 专门统计重载行驶格数与空载行驶格数，为整个调度系统的匈牙利效能优化提供精确的底层离散数学数据支撑。
 */
void Robot::recordGridMovement() {
    // 只有在实际发生网格逻辑位移的标称作业状态下才执行累加计数
    if (status == RobotStatus::MOVING_TO_PICK || status == RobotStatus::RETURNING_BUFFER) {
        totalGridsWalked++; // 系统总累计里程数递增
        emptyGridsWalked++; // 空载行驶里程数递增
    }
    else if (status == RobotStatus::MOVING_TO_DELIVER) {
        totalGridsWalked++; // 系统总累计里程数递增（此时属于重载运送状态）
    }
}