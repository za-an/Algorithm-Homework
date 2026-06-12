// 分支限界法求解 0/1 背包（精确解法）
// 优先队列按分数背包上界出队（最佳优先搜索），
// 用贪心解作为初始下界，上界不超过当前最优值的结点直接剪枝
#ifndef BRANCH_BOUND_KNAPSACK01_H
#define BRANCH_BOUND_KNAPSACK01_H

#include "knapsack01_common.h"

Solution solveBranchBound(const Instance& instance);

#endif
