// 动态规划求解 0/1 背包（精确解法）
// solveDp2d     - 教科书式二维状态表 dp[i][c]，直接回溯得到选择集合，
//                 时间 O(n*C)，空间 O(n*C)（每个状态 8 字节）
// solveDpRolling - 一维滚动数组空间优化，结合分治思想恢复选择集合，
//                 时间约 2*O(n*C)，空间 O(C)
#ifndef DP_KNAPSACK01_H
#define DP_KNAPSACK01_H

#include "knapsack01_common.h"

Solution solveDp2d(const Instance& instance);
Solution solveDpRolling(const Instance& instance);

#endif
