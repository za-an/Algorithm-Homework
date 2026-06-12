#include "backtracking-knapsack01.h"

#include <algorithm>
#include <string>

#include "greedy-knapsack01.h"

using namespace std;

Solution solveBacktracking(const Instance& instance) {
    const vector<Item> items = sortedByRatio(instance.items);

    Solution best;
    best.algorithm = "backtracking";
    best.optimal = true;

    // 贪心解作为初始下界（热启动），搜索一开始就能剪掉大量分支
    const Solution warmStart = solveGreedy(instance);
    best.value = warmStart.value;
    best.weight = warmStart.weight;
    best.selectedItems = warmStart.selectedItems;

    ll nodes = 0;
    ll pruned = 0;
    vector<int> currentSelected;

    auto dfs = [&](auto&& self, int level, ll currentWeight, ll currentValue) -> void {
        ++nodes;

        if (currentValue > best.value) {
            best.value = currentValue;
            best.weight = currentWeight;
            best.selectedItems = currentSelected;
        }

        if (level >= static_cast<int>(items.size())) {
            return;
        }

        const double bound = fractionalUpperBound(
            items,
            level,
            instance.capacity - currentWeight,
            currentValue
        );
        if (bound <= static_cast<double>(best.value)) {
            ++pruned;
            return;
        }

        const Item& item = items[level];
        if (currentWeight + item.weight <= instance.capacity) {
            currentSelected.push_back(item.index);
            self(self, level + 1, currentWeight + item.weight, currentValue + item.value);
            currentSelected.pop_back();
        }
        self(self, level + 1, currentWeight, currentValue);
    };

    dfs(dfs, 0, 0, 0);
    sort(best.selectedItems.begin(), best.selectedItems.end());
    best.details = "nodes=" + to_string(nodes) + ";pruned=" + to_string(pruned) +
                   ";warm_start=greedy(" + to_string(warmStart.value) + ")";
    best.memoryBytes = sizeof(Item) * items.size() + sizeof(int) * items.size() * 2;
    return best;
}
