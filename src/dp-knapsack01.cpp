#include "dp-knapsack01.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace std;

namespace {

constexpr ll kMaxDpCapacity = 20000000;

void checkCapacity(const Instance& instance) {
    if (instance.capacity > kMaxDpCapacity) {
        throw runtime_error("capacity too large for DP table; use a smaller dataset");
    }
}

// 用一维滚动数组计算物品区间 [lo, hi) 在容量 cap 下的最优值表
vector<ll> rollingTable(const vector<Item>& items, int lo, int hi, ll cap) {
    vector<ll> dp(static_cast<size_t>(cap) + 1, 0);
    for (int i = lo; i < hi; ++i) {
        const ll weight = items[i].weight;
        const ll value = items[i].value;
        if (weight > cap) {
            continue;
        }
        for (ll c = cap; c >= weight; --c) {
            const ll candidate = dp[static_cast<size_t>(c - weight)] + value;
            if (candidate > dp[static_cast<size_t>(c)]) {
                dp[static_cast<size_t>(c)] = candidate;
            }
        }
    }
    return dp;
}

// 分治恢复选择集合：找到容量在前后两半物品之间的最优分配点后分别递归。
// 每层总开销 O(n*C) 且容量随递归减半分摊，总时间约 2*O(n*C)，
// 任意时刻只保留 O(C) 个状态。
void recoverSelection(
    const vector<Item>& items,
    int lo,
    int hi,
    ll cap,
    vector<int>& selected
) {
    if (lo >= hi || cap <= 0) {
        return;
    }
    if (hi - lo == 1) {
        const Item& item = items[lo];
        if (item.weight <= cap && item.value > 0) {
            selected.push_back(item.index);
        }
        return;
    }

    const int mid = lo + (hi - lo) / 2;
    ll bestSplit = 0;
    {
        const vector<ll> left = rollingTable(items, lo, mid, cap);
        const vector<ll> right = rollingTable(items, mid, hi, cap);
        ll bestValue = -1;
        for (ll c = 0; c <= cap; ++c) {
            const ll combined = left[static_cast<size_t>(c)] +
                                right[static_cast<size_t>(cap - c)];
            if (combined > bestValue) {
                bestValue = combined;
                bestSplit = c;
            }
        }
    }
    recoverSelection(items, lo, mid, bestSplit, selected);
    recoverSelection(items, mid, hi, cap - bestSplit, selected);
}

void fillValueAndWeight(const Instance& instance, Solution& solution) {
    sort(solution.selectedItems.begin(), solution.selectedItems.end());
    for (int index : solution.selectedItems) {
        const Item& item = instance.items[static_cast<size_t>(index - 1)];
        solution.weight += item.weight;
        solution.value += item.value;
    }
}

}  // namespace

Solution solveDp2d(const Instance& instance) {
    checkCapacity(instance);

    const size_t n = instance.items.size();
    const auto capacity = static_cast<size_t>(instance.capacity);
    vector<vector<ll>> dp(n + 1, vector<ll>(capacity + 1, 0));

    for (size_t i = 1; i <= n; ++i) {
        const Item& item = instance.items[i - 1];
        const auto weight = static_cast<size_t>(item.weight);
        for (size_t c = 0; c <= capacity; ++c) {
            dp[i][c] = dp[i - 1][c];
            if (weight <= c) {
                const ll candidate = dp[i - 1][c - weight] + item.value;
                if (candidate > dp[i][c]) {
                    dp[i][c] = candidate;
                }
            }
        }
    }

    Solution solution;
    solution.algorithm = "dynamic_programming_2d";
    solution.optimal = true;
    solution.details = "table=2d;states=" + to_string(n * (capacity + 1)) +
                       ";bytes_per_state=" + to_string(sizeof(ll));

    size_t c = capacity;
    for (size_t i = n; i >= 1; --i) {
        if (dp[i][c] != dp[i - 1][c]) {
            const Item& item = instance.items[i - 1];
            solution.selectedItems.push_back(item.index);
            c -= static_cast<size_t>(item.weight);
        }
    }
    fillValueAndWeight(instance, solution);
    solution.memoryBytes = sizeof(ll) * (n + 1) * (capacity + 1);
    return solution;
}

Solution solveDpRolling(const Instance& instance) {
    checkCapacity(instance);

    const size_t n = instance.items.size();
    const auto capacity = static_cast<size_t>(instance.capacity);

    Solution solution;
    solution.algorithm = "dynamic_programming";
    solution.optimal = true;
    solution.details = "table=1d_rolling;divide_and_conquer_recovery;rolling_array_size=" +
                       to_string(capacity + 1);

    recoverSelection(
        instance.items,
        0,
        static_cast<int>(n),
        instance.capacity,
        solution.selectedItems
    );
    fillValueAndWeight(instance, solution);

    // 峰值内存：顶层分治同时保留两个 (capacity+1) 的滚动数组
    solution.memoryBytes = sizeof(ll) * (capacity + 1) * 2 +
                           sizeof(int) * solution.selectedItems.size();
    return solution;
}
