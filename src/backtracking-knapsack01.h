// 回溯法求解 0/1 背包（精确解法）
// 在按价值密度排序的解空间树上做深度优先搜索，
// 用贪心解作为初始下界，用分数背包上界剪枝
#ifndef BACKTRACKING_KNAPSACK01_H
#define BACKTRACKING_KNAPSACK01_H

#include "knapsack01_common.h"

Solution solveBacktracking(const Instance& instance);

#endif
