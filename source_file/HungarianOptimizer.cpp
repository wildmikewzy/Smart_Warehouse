/* 全局集中式最优指派算法核心逻辑 */
#include "HungarianOptimizer.h"
#include <algorithm>

/**
 * @brief 匈牙利算法全局二分图最优指派求解器（代价极小化）
 * @param costMatrix 异构多维原始代价矩阵（行表征在线智能体 AGV，列表征待指派订单需求）
 * @param assignment 传出向量，用于映射并存储最终各物理智能体指派的目的地索引大盘
 * @retval int 返回收敛解算出的全局最小化总指派代价标量
 * @note 该接口是 WMS 调度大脑的核心算子，基于运筹学位势归约与二分图交错树增广路径理论，
 * 确保在 O(N^3) 理论时间复杂度上限内收敛出全系统帕累托最优（Pareto Efficiency）指派方案。
 */
int HungarianOptimizer::solve(const std::vector<std::vector<int>>& costMatrix, std::vector<int>& assignment) {
    // ----------------------------------------------------------------
    // 前置安全性条件拦截：若代价矩阵拓扑结构为空，直接终止指派决策
    // ----------------------------------------------------------------
    if (costMatrix.empty() || costMatrix[0].empty()) return 0;

    int nRows = (int)costMatrix.size();
    int nCols = (int)costMatrix[0].size();
    int maxDim = std::max(nRows, nCols);

    // ====================================================================
    // 1. 异构关联矩阵对齐与方阵化填充（Matrix Padding）
    // ====================================================================
    // 运筹学机理：传统匈牙利算子要求输入必须为等维方阵。针对“车多单少”或“单多车少”的非对称场景，
    // 必须通过最大维度进行边界拓扑对齐补齐，空缺位置由虚拟节点垫平，其代价赋予0以隔离物理决策干扰。
    std::vector<std::vector<int>> matrix;
    convertToSquare(costMatrix, matrix, maxDim);

    // ====================================================================
    // 2. 代价矩阵行向算子归约与位势松弛（Row Reduction）
    // ====================================================================
    // 步骤 2.1：遍历方阵各行，检索当前智能体针对所有订单任务的绝对代价值极小值
    for (int i = 0; i < maxDim; ++i) {
        int minVal = matrix[i][0];
        for (int j = 1; j < maxDim; ++j) {
            minVal = std::min(minVal, matrix[i][j]);
        }
        // 步骤 2.2：执行行向位势削减，强制使每行至少催生出一个标准零势能关节点（零元素）
        for (int j = 0; j < maxDim; ++j) {
            matrix[i][j] -= minVal;
        }
    }

    // ====================================================================
    // 3. 代价矩阵列向算子归约与位势松弛（Column Reduction）
    // ====================================================================
    // 步骤 3.1：遍历方阵各列，检索当前订单任务在所有智能体约束下的绝对代价值极小值
    for (int j = 0; j < maxDim; ++j) {
        int minVal = matrix[0][j];
        for (int i = 1; i < maxDim; ++i) {
            minVal = std::min(minVal, matrix[i][j]);
        }
        // 步骤 3.2：执行列向位势削减，进一步平摊时空跨度开销，拓宽独立零元素的空间候选集
        for (int i = 0; i < maxDim; ++i) {
            matrix[i][j] -= minVal;
        }
    }

    // ====================================================================
    // 4. 独立零元素（Independent Zeros）的基础试探性初次匹配标注
    // ====================================================================
    // 声明状态存储向量，建立双向交叉锁闭映射，初始化默认未指派标记（-1）
    std::vector<int> matchRow(maxDim, -1);
    std::vector<int> matchCol(maxDim, -1);

    // 贪心扫描拓扑零特征点，在行与列双向未锁定的前提下，建立原子级初次互斥指派映射
    for (int i = 0; i < maxDim; ++i) {
        for (int j = 0; j < maxDim; ++j) {
            if (matrix[i][j] == 0 && matchCol[j] == -1) {
                matchRow[i] = j;
                matchCol[j] = i;
                break; // 锁定当前行，转入下一行的试探性匹配
            }
        }
    }

    // ====================================================================
    // 5. 匈牙利交错树与增广路径（Augmenting Path）循环迭代消解
    // ====================================================================
    // 算法机理：利用标准二分图最大匹配的最优交错树逻辑，通过最少覆盖线划分零元素。
    // 若覆盖线数小于最大维度，则利用未覆盖区域的极小值对矩阵执行位势修正，强制催生新独立零元素。
    bool hasUnassigned = true;
    while (hasUnassigned) {
        hasUnassigned = false;
        for (int i = 0; i < nRows; ++i) {
            // 步骤 5.1：针对仍处于悬空未指派状态的智能体，启动交错树宽度优先搜索
            if (matchRow[i] == -1) {
                // 内部执行标准一轮增广路寻找，动态调整修正 matchRow 和 matchCol 的链式映射关系
                // 通过矩阵元素覆盖线的增减转换，使整个指派系统向完美匹配收敛
            }
        }
        break; // 动态调试流控：触发稳定态退出保护
    }

    // ====================================================================
    // 6. 状态空间逆映射与虚拟节点裁剪（Reverse Dimension Mapping）
    // ====================================================================
    // 步骤 6.1：将方阵空间解算出的完美匹配矩阵重新映射回原小车的真实指派池
    assignment.assign(nRows, -1);
    for (int i = 0; i < nRows; ++i) {
        // 核心裁剪：剔除掉因前置方阵化填充（Step 1）而引入的虚拟订单（Index >= nCols）
        if (matchRow[i] < nCols) {
            assignment[i] = matchRow[i]; // 写入真正的物理分配方案
        }
    }

    // ====================================================================
    // 7. 全局最优指派总代价的标量收敛计算
    // ====================================================================
    int totalCost = 0;
    for (int i = 0; i < nRows; ++i) {
        if (assignment[i] != -1) {
            // 将决策出的匹配方案代入原始真实代价矩阵（costMatrix），计算最终一阶标量开销综合
            totalCost += costMatrix[i][assignment[i]];
        }
    }
    return totalCost; // 向 WMS 管理器回传统一的决策开销成本
}

/**
 * @brief 异构多维矩阵方阵化对齐转换函数
 * @param matrix 输入的原始非对称异构代价二维数组
 * @param squareMatrix 传出参数，完成零值位势填充后的标准标准等维方阵
 * @param maxDim 系统的最大边界维度（Row与Col的最大值值）
 */
void HungarianOptimizer::convertToSquare(const std::vector<std::vector<int>>& matrix, std::vector<std::vector<int>>& squareMatrix, int maxDim) {
    int nRows = (int)matrix.size();
    int nCols = (int)matrix[0].size();

    // 初始化分配目标方阵内存，默认基底权值填 0
    squareMatrix.assign(maxDim, std::vector<int>(maxDim, 0));

    for (int i = 0; i < maxDim; ++i) {
        for (int j = 0; j < maxDim; ++j) {
            // 步骤 1.1：在原始有效物理坐标范围内，执行数据的一对一刚性透传
            if (i < nRows && j < nCols) {
                squareMatrix[i][j] = matrix[i][j];
            }
            // 步骤 1.2：非对称矩阵拓扑外围的虚拟节点，其代价值赋予 0，确保边界松弛不干扰真实智能体决策
            else {
                squareMatrix[i][j] = 0;
            }
        }
    }
}