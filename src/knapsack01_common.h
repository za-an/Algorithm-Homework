// 0/1 背包实验公共定义：数据结构、排序、上界估计与解校验
#ifndef KNAPSACK01_COMMON_H
#define KNAPSACK01_COMMON_H

#include <cstddef>
#include <string>
#include <vector>

using ll = long long;

struct Item {
    int index{};
    ll weight{};
    ll value{};

    double ratio() const {
        return static_cast<double>(value) / static_cast<double>(weight);
    }
};

struct Instance {
    std::string name;
    std::string source;
    ll capacity{};
    ll knownOptimum{-1};
    long double valueScale{1.0L};
    long double weightScale{1.0L};
    std::vector<Item> items;
};

struct Solution {
    std::string algorithm;
    ll value{};
    ll weight{};
    std::vector<int> selectedItems;
    bool optimal{};
    std::size_t memoryBytes{};
    std::string details;
};

// 按价值密度（value/weight）降序排序，回溯法、分支限界法和贪心法共用
std::vector<Item> sortedByRatio(const std::vector<Item>& items);

// 分数背包松弛上界：从 start 开始按密度装入，最后一件可分割
double fractionalUpperBound(
    const std::vector<Item>& items,
    int start,
    ll capacityLeft,
    ll currentValue
);

// 校验解的可行性与一致性，不通过时抛出异常
void validateSolution(const Instance& instance, const Solution& solution);

#endif
