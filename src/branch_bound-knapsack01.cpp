#include "branch_bound-knapsack01.h"

#include <algorithm>
#include <queue>
#include <string>
#include <utility>

#include "greedy-knapsack01.h"

using namespace std;

namespace {

struct Node {
    int level{};
    ll value{};
    ll weight{};
    double bound{};
    vector<int> selectedItems;
};

struct NodeLess {
    bool operator()(const Node& a, const Node& b) const {
        return a.bound < b.bound;
    }
};

}  // namespace

Solution solveBranchBound(const Instance& instance) {
    const vector<Item> items = sortedByRatio(instance.items);
    priority_queue<Node, vector<Node>, NodeLess> queue;

    Solution best;
    best.algorithm = "branch_bound";
    best.optimal = true;

    // 贪心解作为初始下界（热启动），减少入队的无效结点
    const Solution warmStart = solveGreedy(instance);
    best.value = warmStart.value;
    best.weight = warmStart.weight;
    best.selectedItems = warmStart.selectedItems;

    Node start;
    start.bound = fractionalUpperBound(items, 0, instance.capacity, 0);
    queue.push(start);

    ll nodes = 0;
    ll pruned = 0;
    size_t maxQueueSize = 1;

    while (!queue.empty()) {
        maxQueueSize = max(maxQueueSize, queue.size());
        Node node = queue.top();
        queue.pop();
        ++nodes;

        if (node.bound <= static_cast<double>(best.value) ||
            node.level >= static_cast<int>(items.size())) {
            ++pruned;
            continue;
        }

        const Item& item = items[node.level];
        if (node.weight + item.weight <= instance.capacity) {
            Node include = node;
            include.level = node.level + 1;
            include.weight += item.weight;
            include.value += item.value;
            include.selectedItems.push_back(item.index);

            if (include.value > best.value) {
                best.value = include.value;
                best.weight = include.weight;
                best.selectedItems = include.selectedItems;
            }

            include.bound = fractionalUpperBound(
                items,
                include.level,
                instance.capacity - include.weight,
                include.value
            );
            if (include.bound > static_cast<double>(best.value)) {
                queue.push(move(include));
            } else {
                ++pruned;
            }
        }

        Node exclude = node;
        exclude.level = node.level + 1;
        exclude.bound = fractionalUpperBound(
            items,
            exclude.level,
            instance.capacity - exclude.weight,
            exclude.value
        );
        if (exclude.bound > static_cast<double>(best.value)) {
            queue.push(move(exclude));
        } else {
            ++pruned;
        }
    }

    sort(best.selectedItems.begin(), best.selectedItems.end());
    best.details = "nodes=" + to_string(nodes) +
                   ";pruned=" + to_string(pruned) +
                   ";max_queue_size=" + to_string(maxQueueSize) +
                   ";warm_start=greedy(" + to_string(warmStart.value) + ")";
    best.memoryBytes = sizeof(Item) * items.size() +
                       maxQueueSize * (sizeof(Node) + sizeof(int) * items.size());
    return best;
}
