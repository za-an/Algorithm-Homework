#include "knapsack01_common.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std;

vector<Item> sortedByRatio(const vector<Item>& items) {
    vector<Item> sorted = items;
    sort(sorted.begin(), sorted.end(), [](const Item& a, const Item& b) {
        if (abs(a.ratio() - b.ratio()) > 1e-12) {
            return a.ratio() > b.ratio();
        }
        if (a.value != b.value) {
            return a.value > b.value;
        }
        if (a.weight != b.weight) {
            return a.weight < b.weight;
        }
        return a.index < b.index;
    });
    return sorted;
}

double fractionalUpperBound(
    const vector<Item>& items,
    int start,
    ll capacityLeft,
    ll currentValue
) {
    if (capacityLeft < 0) {
        return -numeric_limits<double>::infinity();
    }

    double bound = static_cast<double>(currentValue);
    ll remaining = capacityLeft;
    for (int i = start; i < static_cast<int>(items.size()); ++i) {
        const Item& item = items[i];
        if (item.weight <= remaining) {
            remaining -= item.weight;
            bound += static_cast<double>(item.value);
        } else {
            bound += static_cast<double>(item.value) * static_cast<double>(remaining) /
                     static_cast<double>(item.weight);
            break;
        }
    }
    return bound;
}

void validateSolution(const Instance& instance, const Solution& solution) {
    vector<char> seen(instance.items.size() + 1, 0);
    ll weight = 0;
    ll value = 0;

    for (int index : solution.selectedItems) {
        if (index <= 0 || index > static_cast<int>(instance.items.size())) {
            throw runtime_error(solution.algorithm + " selected invalid item index");
        }
        if (seen[static_cast<size_t>(index)] != 0) {
            throw runtime_error(solution.algorithm + " selected duplicate item");
        }
        seen[static_cast<size_t>(index)] = 1;
        const Item& item = instance.items[static_cast<size_t>(index - 1)];
        weight += item.weight;
        value += item.value;
    }

    if (weight != solution.weight || value != solution.value) {
        throw runtime_error(solution.algorithm + " value/weight mismatch");
    }
    if (weight > instance.capacity) {
        throw runtime_error(solution.algorithm + " exceeds capacity");
    }
}
