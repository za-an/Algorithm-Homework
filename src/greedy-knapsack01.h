// 贪心算法求解 0/1 背包（近似解法）
// 支持三种局部选择准则：
//   ratio  - 按单位重量价值（价值密度）降序
//   value  - 按物品价值降序
//   weight - 按物品重量升序
#ifndef GREEDY_KNAPSACK01_H
#define GREEDY_KNAPSACK01_H

#include <string>

#include "knapsack01_common.h"

Solution solveGreedy(const Instance& instance, const std::string& criterion = "ratio");

#endif
