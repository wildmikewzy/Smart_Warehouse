/*机器人的逻辑执行*/
#pragma once
#include "common.h"
#include <vector>

// 机器人类
class Robot {
public:
    // 构造函数，初始化机器人的ID号和初始位置
    Robot(int _id, Point startPos);

    void setPath(const std::vector<Point>& newPath);        // 设置路径函数，传入新的路径数据
    void update();  // 更新函数，主函数中调用

    static std::vector<Robot*>* allRobots;      // 静态机器人列表
    static void setRobotList(std::vector<Robot*>* list);

    // 公开成员
    int id;
    Point currentPos;     // 逻辑所在的网格 (int)
    Point homePos;        // 记录小车初始待命点的专属坐标
    RobotStatus status;
    float currentSpeed;   // 移动速度

    // 用于 GUI 平滑渲染的视觉坐标,float 类型允许机器人在格点之间平滑过渡
    float realX;
    float realY;

    // 路径数据
    std::vector<Point> pathQueue;       // 路径列表
    std::vector<Point> historyPath;     // 历史路径列表，记录已经走过的路径点

    // 离散格点移动控制
    int moveCooldown;       // 当前格子的剩余停留帧数
    int currentStepFrames;  // 当前速度级别下，走一格所需的标准帧数（越小越快）

    // 新增工作状态冷却时间与绑定的订单任务
    int workCooldown = 0;   // 操作冷却（如装载卸载的时间）
    int currentTargetStationId = -1;  // 当前正在处理的站点目标

    // 当前绑定的业务订单源 ID
    int currentOrderId = -1;

    // === 【新修复加入：平滑动线控制核心】 ===
    Point lastPos;               // 记录当前运动网格区间的“起点格”
    float moveProgress = 0.0f;   // 当前格子区间的平滑进度 (0.0f 到 1.0f)
    bool cellStepCompleted = false; // 标志位：是否在当前帧刚刚完成了“一整格”的跨越
};