#include "greedy-knapsack01.h"

#include <algorithm>
#include <stdexcept>

using namespace std;

static vector<Item> sortedByCriterion(const vector<Item>& items, const string& criterion) {
    if (criterion == "ratio") {
        return sortedByRatio(items);
    }

    vector<Item> sorted = items;
    if (criterion == "value") {
        sort(sorted.begin(), sorted.end(), [](const Item& a, const Item& b) {
            if (a.value != b.value) {
                return a.value > b.value;
            }
            if (a.weight != b.weight) {
                return a.weight < b.weight;
            }
            return a.index < b.index;
        });
    } else if (criterion == "weight") {
        sort(sorted.begin(), sorted.end(), [](const Item& a, const Item& b) {
            if (a.weight != b.weight) {
                return a.weight < b.weight;
            }
            if (a.value != b.value) {
                return a.value > b.value;
            }
            return a.index < b.index;
        });
    } else {
        throw runtime_error("unknown greedy criterion: " + criterion);
    }
    return sorted;
}

Solution solveGreedy(const Instance& instance, const string& criterion) {
    Solution solution;
    solution.algorithm = criterion == "ratio" ? "greedy" : "greedy_" + criterion;
    solution.optimal = false;
    solution.details = "criterion=" + (criterion == "ratio" ? string("value/weight") : criterion);

    ll remaining = instance.capacity;
    for (const Item& item : sortedByCriterion(instance.items, criterion)) {
        if (item.weight <= remaining) {
            remaining -= item.weight;
            solution.weight += item.weight;
            solution.value += item.value;
            solution.selectedItems.push_back(item.index);
        }
    }
    sort(solution.selectedItems.begin(), solution.selectedItems.end());
    solution.memoryBytes = sizeof(Item) * instance.items.size() * 2 +
                           sizeof(int) * solution.selectedItems.size();
    return solution;
}
