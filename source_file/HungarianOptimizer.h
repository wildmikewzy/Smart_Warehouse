#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class HungarianOptimizer {
public:
    /**
     * @brief 求解全局最低代价匹配
     * @param costMatrix 痛苦指数代价矩阵（行:空闲小车，列:等待订单）
     * @param assignment 返回的匹配结果：assignment[i] 存储第 i 辆小车分配到的订单索引。若未分配则为 -1
     * @return int 全局总代价
     */
    int solve(const std::vector<std::vector<int>>& costMatrix, std::vector<int>& assignment);

private:
    void convertToSquare(const std::vector<std::vector<int>>& matrix, std::vector<std::vector<int>>& squareMatrix, int maxDim);
};