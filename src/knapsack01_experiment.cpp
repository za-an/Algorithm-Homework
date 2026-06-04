#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using ll = long long;
namespace fs = std::filesystem;

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

struct Record {
    std::string source;
    std::string dataset;
    int itemCount{};
    ll capacity{};
    ll knownOptimum{-1};
    long double valueScale{1.0L};
    long double weightScale{1.0L};
    std::string algorithm;
    std::string status;
    ll value{};
    ll weight{};
    std::vector<int> selectedItems;
    double timeMs{};
    double memoryKb{};
    bool optimal{};
    ll referenceOptimum{};
    double qualityRatio{-1.0};
    std::string details;
};

static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

static std::vector<std::string> splitWords(std::string line) {
    std::replace(line.begin(), line.end(), ',', ' ');
    std::istringstream input(line);
    std::vector<std::string> parts;
    std::string part;
    while (input >> part) {
        parts.push_back(part);
    }
    return parts;
}

static std::string joinIndexes(const std::vector<int>& values, const std::string& sep = " ") {
    std::ostringstream output;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << sep;
        }
        output << values[i];
    }
    return output.str();
}

static std::string csvEscape(const std::string& value) {
    bool needQuotes = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!needQuotes) {
        return value;
    }

    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
}

static std::string htmlEscape(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

static bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

static std::vector<ll> readNumbersFromFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open file: " + path.string());
    }

    std::vector<ll> numbers;
    std::string token;
    while (file >> token) {
        if (!token.empty() && token[0] == '#') {
            std::string ignored;
            std::getline(file, ignored);
            continue;
        }
        numbers.push_back(std::stoll(token));
    }
    return numbers;
}

static std::vector<std::string> readNumberTokensFromFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open file: " + path.string());
    }

    std::vector<std::string> tokens;
    std::string token;
    while (file >> token) {
        if (!token.empty() && token[0] == '#') {
            std::string ignored;
            std::getline(file, ignored);
            continue;
        }
        tokens.push_back(token);
    }
    return tokens;
}

static bool hasDecimalToken(const std::string& token) {
    return token.find('.') != std::string::npos ||
           token.find('e') != std::string::npos ||
           token.find('E') != std::string::npos;
}

static ll parseScaled(const std::string& token, long double scale) {
    return static_cast<ll>(std::llround(std::stold(token) * scale));
}

static std::string formatScaled(ll value, long double scale, int precision = 4) {
    if (scale == 1.0L) {
        return std::to_string(value);
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(precision)
           << static_cast<long double>(value) / scale;
    std::string text = output.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

static std::string formatRatio(double value) {
    if (value < 0.0) {
        return "";
    }
    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

static void validateInstance(const Instance& instance, const fs::path& path) {
    if (instance.capacity < 0) {
        throw std::runtime_error("capacity must be non-negative in " + path.string());
    }
    if (instance.items.empty()) {
        throw std::runtime_error("empty instance in " + path.string());
    }
    for (const Item& item : instance.items) {
        if (item.weight <= 0 || item.value < 0) {
            throw std::runtime_error("invalid item weight/value in " + path.string());
        }
    }
}

static Instance readSimpleInstance(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open dataset: " + path.string());
    }

    Instance instance;
    instance.name = path.stem().string();
    instance.source = "FSU";
    bool hasCapacity = false;
    std::string capacityToken;
    std::string knownOptimumToken;
    std::vector<std::vector<std::string>> tokenRows;

    std::string rawLine;
    while (std::getline(file, rawLine)) {
        const auto comment = rawLine.find('#');
        if (comment != std::string::npos) {
            const std::string commentText = trim(rawLine.substr(comment + 1));
            const std::string lowerComment = toLower(commentText);
            if (startsWith(lowerComment, "source:")) {
                instance.source = trim(commentText.substr(std::string("source:").size()));
            } else if (startsWith(lowerComment, "optimal value:")) {
                knownOptimumToken = trim(commentText.substr(std::string("optimal value:").size()));
            }
            rawLine = rawLine.substr(0, comment);
        }

        const std::string line = trim(rawLine);
        if (line.empty()) {
            continue;
        }

        const auto parts = splitWords(line);
        if (parts.empty()) {
            continue;
        }

        std::string key = toLower(parts[0]);
        if (!key.empty() && key.back() == ':') {
            key.pop_back();
        }

        if (key == "name" && parts.size() >= 2) {
            instance.name.clear();
            for (std::size_t i = 1; i < parts.size(); ++i) {
                if (i > 1) {
                    instance.name += " ";
                }
                instance.name += parts[i];
            }
            continue;
        }
        if ((key == "capacity" || key == "cap") && parts.size() >= 2) {
            capacityToken = parts[1];
            hasCapacity = true;
            continue;
        }
        if (key == "n" || key == "items") {
            continue;
        }

        try {
            for (const auto& part : parts) {
                std::size_t parsed = 0;
                std::stold(part, &parsed);
                if (parsed != part.size()) {
                    throw std::invalid_argument("trailing characters");
                }
            }
        } catch (const std::exception&) {
            throw std::runtime_error("cannot parse line in " + path.string() + ": " + line);
        }
        tokenRows.push_back(parts);
    }

    std::vector<std::vector<std::string>> itemRows;
    if (!hasCapacity) {
        if (tokenRows.empty() || tokenRows[0].size() < 2) {
            throw std::runtime_error("missing header n capacity in " + path.string());
        }
        const ll declaredN = std::stoll(tokenRows[0][0]);
        capacityToken = tokenRows[0][1];
        itemRows.assign(tokenRows.begin() + 1, tokenRows.end());
        if (declaredN != static_cast<ll>(itemRows.size())) {
            throw std::runtime_error("item count mismatch in " + path.string());
        }
    } else {
        itemRows = tokenRows;
    }

    bool hasDecimalWeight = hasDecimalToken(capacityToken);
    bool hasDecimalValue = hasDecimalToken(knownOptimumToken);
    for (const auto& row : itemRows) {
        if (row.size() < 2) {
            throw std::runtime_error("item row must contain weight and value in " + path.string());
        }
        hasDecimalWeight = hasDecimalWeight || hasDecimalToken(row[0]);
        hasDecimalValue = hasDecimalValue || hasDecimalToken(row[1]);
    }
    if (hasDecimalWeight) {
        instance.weightScale = 1000000.0L;
    }
    if (hasDecimalValue) {
        instance.valueScale = 1000000.0L;
    }
    instance.capacity = parseScaled(capacityToken, instance.weightScale);
    if (!knownOptimumToken.empty()) {
        instance.knownOptimum = parseScaled(knownOptimumToken, instance.valueScale);
    }

    for (std::size_t i = 0; i < itemRows.size(); ++i) {
        Item item;
        item.index = static_cast<int>(i + 1);
        item.weight = parseScaled(itemRows[i][0], instance.weightScale);
        item.value = parseScaled(itemRows[i][1], instance.valueScale);
        instance.items.push_back(item);
    }

    if (instance.source.find("Florida State University") != std::string::npos) {
        instance.source = "FSU KNAPSACK_01";
    }
    validateInstance(instance, path);
    return instance;
}

static Instance readUnicaucaInstance(const fs::path& path, const fs::path& optimumPath) {
    const std::vector<std::string> tokens = readNumberTokensFromFile(path);
    if (tokens.size() < 2) {
        throw std::runtime_error("invalid Unicauca instance: " + path.string());
    }

    Instance instance;
    instance.name = path.filename().string();
    instance.source = startsWith(path.parent_path().filename().string(), "large")
        ? "Unicauca large_scale"
        : "Unicauca low-dimensional";

    const ll declaredN = std::stoll(tokens[0]);
    const std::size_t itemTokenCount = static_cast<std::size_t>(2 + declaredN * 2);
    const std::size_t itemAndSelectionTokenCount = static_cast<std::size_t>(2 + declaredN * 3);
    if (tokens.size() != itemTokenCount && tokens.size() != itemAndSelectionTokenCount) {
        throw std::runtime_error("item count mismatch in " + path.string());
    }

    bool hasDecimal = false;
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        hasDecimal = hasDecimal || hasDecimalToken(tokens[i]);
    }
    if (hasDecimal) {
        instance.valueScale = 1000000.0L;
        instance.weightScale = 1000000.0L;
    }

    instance.capacity = parseScaled(tokens[1], instance.weightScale);
    for (ll i = 0; i < declaredN; ++i) {
        const ll value = parseScaled(tokens[static_cast<std::size_t>(2 + i * 2)], instance.valueScale);
        const ll weight = parseScaled(tokens[static_cast<std::size_t>(2 + i * 2 + 1)], instance.weightScale);
        instance.items.push_back({static_cast<int>(i + 1), weight, value});
    }

    if (fs::exists(optimumPath)) {
        const auto optimumTokens = readNumberTokensFromFile(optimumPath);
        if (!optimumTokens.empty()) {
            instance.knownOptimum = parseScaled(optimumTokens[0], instance.valueScale);
        }
    }

    validateInstance(instance, path);
    return instance;
}

static Instance readJjInstance(
    const fs::path& testPath,
    const std::unordered_map<std::string, ll>& optima
) {
    const std::vector<ll> numbers = readNumbersFromFile(testPath);
    if (numbers.size() < 2) {
        throw std::runtime_error("invalid JorikJooken instance: " + testPath.string());
    }

    Instance instance;
    instance.name = testPath.parent_path().filename().string();
    instance.source = "JorikJooken";

    const ll declaredN = numbers[0];
    if (numbers.size() != static_cast<std::size_t>(1 + declaredN * 3 + 1)) {
        throw std::runtime_error("item count mismatch in " + testPath.string());
    }

    for (ll i = 0; i < declaredN; ++i) {
        const std::size_t offset = static_cast<std::size_t>(1 + i * 3);
        const ll id = numbers[offset];
        const ll value = numbers[offset + 1];
        const ll weight = numbers[offset + 2];
        instance.items.push_back({static_cast<int>(id + 1), weight, value});
    }
    instance.capacity = numbers.back();

    const auto optimum = optima.find(instance.name);
    if (optimum != optima.end() && optimum->second > 0) {
        instance.knownOptimum = optimum->second;
    }

    validateInstance(instance, testPath);
    return instance;
}

static std::vector<Item> sortedByRatio(const std::vector<Item>& items) {
    std::vector<Item> sorted = items;
    std::sort(sorted.begin(), sorted.end(), [](const Item& a, const Item& b) {
        if (std::abs(a.ratio() - b.ratio()) > 1e-12) {
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

static double fractionalUpperBound(
    const std::vector<Item>& items,
    int start,
    ll capacityLeft,
    ll currentValue
) {
    if (capacityLeft < 0) {
        return -std::numeric_limits<double>::infinity();
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

static Solution solveGreedy(const Instance& instance) {
    Solution solution;
    solution.algorithm = "greedy";
    solution.optimal = false;
    solution.details = "criterion=value/weight";

    ll remaining = instance.capacity;
    for (const Item& item : sortedByRatio(instance.items)) {
        if (item.weight <= remaining) {
            remaining -= item.weight;
            solution.weight += item.weight;
            solution.value += item.value;
            solution.selectedItems.push_back(item.index);
        }
    }
    std::sort(solution.selectedItems.begin(), solution.selectedItems.end());
    solution.memoryBytes = sizeof(Item) * instance.items.size() * 2 +
                           sizeof(int) * solution.selectedItems.size();
    return solution;
}

static Solution solveDynamicProgramming(const Instance& instance) {
    if (instance.capacity > 5000000) {
        throw std::runtime_error("capacity too large for DP table; use a smaller dataset");
    }

    const auto capacity = static_cast<std::size_t>(instance.capacity);
    std::vector<ll> dp(capacity + 1, 0);
    std::vector<std::vector<unsigned char>> keep(
        instance.items.size(),
        std::vector<unsigned char>(capacity + 1, 0)
    );

    for (std::size_t i = 0; i < instance.items.size(); ++i) {
        const Item& item = instance.items[i];
        if (item.weight > instance.capacity) {
            continue;
        }
        const auto weight = static_cast<std::size_t>(item.weight);
        for (std::size_t c = capacity + 1; c-- > weight;) {
            const ll candidate = dp[c - weight] + item.value;
            if (candidate > dp[c]) {
                dp[c] = candidate;
                keep[i][c] = 1;
            }
        }
    }

    Solution solution;
    solution.algorithm = "dynamic_programming";
    solution.optimal = true;
    solution.details = "states=" + std::to_string(instance.items.size() * (capacity + 1)) +
                       ";rolling_array_size=" + std::to_string(capacity + 1);

    std::size_t c = capacity;
    for (std::size_t i = instance.items.size(); i-- > 0;) {
        if (keep[i][c] != 0) {
            const Item& item = instance.items[i];
            solution.selectedItems.push_back(item.index);
            c -= static_cast<std::size_t>(item.weight);
        }
    }
    std::sort(solution.selectedItems.begin(), solution.selectedItems.end());

    std::vector<char> selected(instance.items.size() + 1, 0);
    for (int index : solution.selectedItems) {
        selected[static_cast<std::size_t>(index)] = 1;
    }
    for (const Item& item : instance.items) {
        if (selected[static_cast<std::size_t>(item.index)] != 0) {
            solution.weight += item.weight;
            solution.value += item.value;
        }
    }
    solution.memoryBytes = sizeof(ll) * dp.size() +
                           sizeof(unsigned char) * keep.size() * (capacity + 1);
    return solution;
}

static Solution solveBacktracking(const Instance& instance) {
    const std::vector<Item> items = sortedByRatio(instance.items);
    Solution best;
    best.algorithm = "backtracking";
    best.optimal = true;

    ll nodes = 0;
    ll pruned = 0;
    std::vector<int> currentSelected;

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
    std::sort(best.selectedItems.begin(), best.selectedItems.end());
    best.details = "nodes=" + std::to_string(nodes) + ";pruned=" + std::to_string(pruned);
    best.memoryBytes = sizeof(Item) * items.size() + sizeof(int) * items.size() * 2;
    return best;
}

struct Node {
    int level{};
    ll value{};
    ll weight{};
    double bound{};
    std::vector<int> selectedItems;
};

struct NodeLess {
    bool operator()(const Node& a, const Node& b) const {
        return a.bound < b.bound;
    }
};

static Solution solveBranchBound(const Instance& instance) {
    const std::vector<Item> items = sortedByRatio(instance.items);
    std::priority_queue<Node, std::vector<Node>, NodeLess> queue;

    Node start;
    start.bound = fractionalUpperBound(items, 0, instance.capacity, 0);
    queue.push(start);

    Solution best;
    best.algorithm = "branch_bound";
    best.optimal = true;

    ll nodes = 0;
    ll pruned = 0;
    std::size_t maxQueueSize = 1;

    while (!queue.empty()) {
        maxQueueSize = std::max(maxQueueSize, queue.size());
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
                queue.push(std::move(include));
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
            queue.push(std::move(exclude));
        } else {
            ++pruned;
        }
    }

    std::sort(best.selectedItems.begin(), best.selectedItems.end());
    best.details = "nodes=" + std::to_string(nodes) +
                   ";pruned=" + std::to_string(pruned) +
                   ";max_queue_size=" + std::to_string(maxQueueSize);
    best.memoryBytes = sizeof(Item) * items.size() +
                       maxQueueSize * (sizeof(Node) + sizeof(int) * items.size());
    return best;
}

static void validateSolution(const Instance& instance, const Solution& solution) {
    std::vector<char> seen(instance.items.size() + 1, 0);
    ll weight = 0;
    ll value = 0;

    for (int index : solution.selectedItems) {
        if (index <= 0 || index > static_cast<int>(instance.items.size())) {
            throw std::runtime_error(solution.algorithm + " selected invalid item index");
        }
        if (seen[static_cast<std::size_t>(index)] != 0) {
            throw std::runtime_error(solution.algorithm + " selected duplicate item");
        }
        seen[static_cast<std::size_t>(index)] = 1;
        const Item& item = instance.items[static_cast<std::size_t>(index - 1)];
        weight += item.weight;
        value += item.value;
    }

    if (weight != solution.weight || value != solution.value) {
        throw std::runtime_error(solution.algorithm + " value/weight mismatch");
    }
    if (weight > instance.capacity) {
        throw std::runtime_error(solution.algorithm + " exceeds capacity");
    }
}

static Solution runAlgorithm(const std::string& name, const Instance& instance) {
    if (name == "greedy") {
        return solveGreedy(instance);
    }
    if (name == "dynamic_programming") {
        return solveDynamicProgramming(instance);
    }
    if (name == "backtracking") {
        return solveBacktracking(instance);
    }
    if (name == "branch_bound") {
        return solveBranchBound(instance);
    }
    throw std::runtime_error("unknown algorithm: " + name);
}

static Record makeSkippedRecord(
    const Instance& instance,
    const std::string& algorithm,
    const std::string& reason
) {
    Record record;
    record.source = instance.source;
    record.dataset = instance.name;
    record.itemCount = static_cast<int>(instance.items.size());
    record.capacity = instance.capacity;
    record.knownOptimum = instance.knownOptimum;
    record.valueScale = instance.valueScale;
    record.weightScale = instance.weightScale;
    record.algorithm = algorithm;
    record.status = "skipped";
    record.optimal = algorithm != "greedy";
    record.details = reason;
    return record;
}

static Record runOne(const Instance& instance, const std::string& algorithm) {
    const auto start = std::chrono::high_resolution_clock::now();
    Solution solution = runAlgorithm(algorithm, instance);
    const auto finish = std::chrono::high_resolution_clock::now();
    validateSolution(instance, solution);

    Record record;
    record.source = instance.source;
    record.dataset = instance.name;
    record.itemCount = static_cast<int>(instance.items.size());
    record.capacity = instance.capacity;
    record.knownOptimum = instance.knownOptimum;
    record.valueScale = instance.valueScale;
    record.weightScale = instance.weightScale;
    record.algorithm = algorithm;
    record.status = "ok";
    record.value = solution.value;
    record.weight = solution.weight;
    record.selectedItems = solution.selectedItems;
    record.timeMs = std::chrono::duration<double, std::milli>(finish - start).count();
    record.memoryKb = static_cast<double>(solution.memoryBytes) / 1024.0;
    record.optimal = solution.optimal;
    record.details = solution.details;
    return record;
}

static std::unordered_map<std::string, ll> loadJjOptima(const fs::path& optimaPath) {
    std::unordered_map<std::string, ll> optima;
    std::ifstream file(optimaPath);
    if (!file) {
        return optima;
    }

    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto comma = line.find(',');
        if (comma == std::string::npos) {
            continue;
        }
        const std::string name = trim(line.substr(0, comma));
        const ll value = std::stoll(trim(line.substr(comma + 1)));
        optima[name] = value;
    }
    return optima;
}

static void appendSimpleInstances(std::vector<Instance>& instances, const fs::path& dataDir) {
    if (!fs::exists(dataDir)) {
        return;
    }

    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        instances.push_back(readSimpleInstance(path));
    }
}

static void appendUnicaucaGroup(
    std::vector<Instance>& instances,
    const fs::path& instanceDir,
    const fs::path& optimumDir
) {
    if (!fs::exists(instanceDir)) {
        return;
    }

    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(instanceDir)) {
        if (entry.is_regular_file()) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        instances.push_back(readUnicaucaInstance(path, optimumDir / path.filename()));
    }
}

static void appendUnicaucaInstances(std::vector<Instance>& instances, const fs::path& uuDir) {
    appendUnicaucaGroup(instances, uuDir / "low-dimensional", uuDir / "low-dimensional-optimum");
    appendUnicaucaGroup(instances, uuDir / "large_scale", uuDir / "large_scale-optimum");
}

static void appendJjInstances(std::vector<Instance>& instances, const fs::path& jjRoot) {
    const fs::path problemDir = jjRoot / "problemInstances";
    if (!fs::exists(problemDir)) {
        return;
    }

    const auto optima = loadJjOptima(jjRoot / "optima.csv");
    std::vector<fs::path> paths;
    for (const auto& entry : fs::recursive_directory_iterator(problemDir)) {
        if (entry.is_regular_file() && entry.path().filename() == "test.in") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        instances.push_back(readJjInstance(path, optima));
    }
}

static std::vector<Instance> loadInstances(
    const std::string& dataDir,
    const std::string& uuDir,
    const std::string& jjRoot,
    const std::vector<std::string>& explicitFiles,
    bool allSources
) {
    std::vector<Instance> instances;

    if (!explicitFiles.empty()) {
        for (const auto& file : explicitFiles) {
            instances.push_back(readSimpleInstance(file));
        }
        return instances;
    }

    appendSimpleInstances(instances, dataDir);
    if (allSources) {
        appendUnicaucaInstances(instances, uuDir);
        appendJjInstances(instances, jjRoot);
    }

    if (instances.empty()) {
        throw std::runtime_error("no dataset files found");
    }
    return instances;
}

static std::vector<Record> runExperiment(
    const std::vector<Instance>& instances,
    const std::vector<std::string>& algorithms,
    int maxExactItems,
    long double maxDpStates
) {
    std::vector<Record> records;

    for (const Instance& instance : instances) {
        std::vector<std::size_t> rowIndexes;

        for (const auto& algorithm : algorithms) {
            rowIndexes.push_back(records.size());
            if (algorithm == "dynamic_programming") {
                const long double states = static_cast<long double>(instance.items.size()) *
                    static_cast<long double>(instance.capacity + 1);
                if (states > maxDpStates) {
                    records.push_back(makeSkippedRecord(
                        instance,
                        algorithm,
                        "states>" + std::to_string(static_cast<ll>(maxDpStates)) +
                            "; use --max-dp-states to run"
                    ));
                    continue;
                }
            }

            if ((algorithm == "backtracking" || algorithm == "branch_bound") &&
                static_cast<int>(instance.items.size()) > maxExactItems) {
                records.push_back(makeSkippedRecord(
                    instance,
                    algorithm,
                    "n>" + std::to_string(maxExactItems) +
                        "; use --max-exact-items to run"
                ));
                continue;
            }
            records.push_back(runOne(instance, algorithm));
        }

        ll reference = instance.knownOptimum > 0 ? instance.knownOptimum : 0;
        for (std::size_t index : rowIndexes) {
            const Record& record = records[index];
            if (record.status == "ok" && record.optimal) {
                reference = std::max(reference, record.value);
            }
        }

        for (std::size_t index : rowIndexes) {
            Record& record = records[index];
            record.referenceOptimum = reference;
            if (record.status == "ok" && reference > 0) {
                record.qualityRatio = static_cast<double>(record.value) /
                                      static_cast<double>(reference);
            }
        }
    }

    return records;
}

static void writeCsv(const std::vector<Record>& records, const fs::path& path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("cannot write " + path.string());
    }

    file.write("\xEF\xBB\xBF", 3);
    file << "数据来源,数据集,物品数量,背包容量,已知最优值,算法,状态,求解价值,总重量,选择物品编号,"
         << "运行时间_ms,估算内存_KB,是否精确算法,参考最优值,解质量比,说明\n";

    for (const Record& record : records) {
        file << csvEscape(record.source) << ','
             << csvEscape(record.dataset) << ','
             << record.itemCount << ','
             << formatScaled(record.capacity, record.weightScale) << ','
             << (record.knownOptimum > 0 ? formatScaled(record.knownOptimum, record.valueScale) : "") << ','
             << csvEscape(record.algorithm) << ','
             << record.status << ',';
        if (record.status == "ok") {
            file << formatScaled(record.value, record.valueScale) << ','
                 << formatScaled(record.weight, record.weightScale) << ','
                 << csvEscape(joinIndexes(record.selectedItems)) << ','
                 << std::fixed << std::setprecision(4) << record.timeMs << ','
                 << std::fixed << std::setprecision(3) << record.memoryKb << ','
                 << (record.optimal ? "true" : "false") << ','
                 << (record.referenceOptimum > 0 ? formatScaled(record.referenceOptimum, record.valueScale) : "") << ','
                 << formatRatio(record.qualityRatio) << ','
                 << csvEscape(record.details) << '\n';
        } else {
            file << ",,,,,"
                 << (record.optimal ? "true" : "false") << ','
                 << (record.referenceOptimum > 0 ? formatScaled(record.referenceOptimum, record.valueScale) : "") << ",,"
                 << csvEscape(record.details) << '\n';
        }
    }
}

static std::string jsonString(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        switch (ch) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += ch;
                break;
        }
    }
    result += '"';
    return result;
}

static void writeJson(const std::vector<Record>& records, const fs::path& path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("cannot write " + path.string());
    }

    file << "[\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const Record& record = records[i];
        file << "  {\n";
        file << "    \"source\": " << jsonString(record.source) << ",\n";
        file << "    \"dataset\": " << jsonString(record.dataset) << ",\n";
        file << "    \"items\": " << record.itemCount << ",\n";
        file << "    \"capacity\": " << formatScaled(record.capacity, record.weightScale) << ",\n";
        file << "    \"known_optimum\": "
             << (record.knownOptimum > 0 ? formatScaled(record.knownOptimum, record.valueScale) : "null")
             << ",\n";
        file << "    \"algorithm\": " << jsonString(record.algorithm) << ",\n";
        file << "    \"status\": " << jsonString(record.status) << ",\n";
        file << "    \"value\": " << formatScaled(record.value, record.valueScale) << ",\n";
        file << "    \"weight\": " << formatScaled(record.weight, record.weightScale) << ",\n";
        file << "    \"selected_items\": [";
        for (std::size_t j = 0; j < record.selectedItems.size(); ++j) {
            if (j > 0) {
                file << ", ";
            }
            file << record.selectedItems[j];
        }
        file << "],\n";
        file << "    \"time_ms\": " << std::fixed << std::setprecision(4) << record.timeMs << ",\n";
        file << "    \"estimated_memory_kb\": " << std::fixed << std::setprecision(3) << record.memoryKb << ",\n";
        file << "    \"optimal\": " << (record.optimal ? "true" : "false") << ",\n";
        file << "    \"reference_optimum\": "
             << (record.referenceOptimum > 0 ? formatScaled(record.referenceOptimum, record.valueScale) : "null")
             << ",\n";
        file << "    \"quality_ratio\": "
             << (record.qualityRatio >= 0.0 ? formatRatio(record.qualityRatio) : "null")
             << ",\n";
        file << "    \"details\": " << jsonString(record.details) << "\n";
        file << "  }" << (i + 1 == records.size() ? "\n" : ",\n");
    }
    file << "]\n";
}

static void writeSummary(const std::vector<Record>& records, const fs::path& path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("cannot write " + path.string());
    }

    file << "# Knapsack Experiment Summary\n\n";
    file << "| Source | Dataset | n | Capacity | Known optimum | Algorithm | Value | Weight | Time ms | Est. Memory KB | Quality | Selected items / reason |\n";
    file << "| --- | --- | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | --- |\n";
    for (const Record& record : records) {
        file << "| " << record.source
             << " | " << record.dataset
             << " | " << record.itemCount
             << " | " << formatScaled(record.capacity, record.weightScale)
             << " | " << (record.knownOptimum > 0 ? formatScaled(record.knownOptimum, record.valueScale) : "")
             << " | " << record.algorithm
             << " | ";
        if (record.status == "ok") {
            file << formatScaled(record.value, record.valueScale)
                 << " | " << formatScaled(record.weight, record.weightScale)
                 << " | " << std::fixed << std::setprecision(4) << record.timeMs
                 << " | " << std::fixed << std::setprecision(3) << record.memoryKb
                 << " | " << formatRatio(record.qualityRatio)
                 << " | " << joinIndexes(record.selectedItems)
                 << " |\n";
        } else {
            file << " | | | | | skipped: " << record.details << " |\n";
        }
    }
}

static double metricValue(const Record& record, const std::string& metric) {
    if (record.status != "ok") {
        return 0.0;
    }
    if (metric == "time_ms") {
        return record.timeMs;
    }
    if (metric == "estimated_memory_kb") {
        return record.memoryKb;
    }
    if (metric == "quality_ratio") {
        return record.qualityRatio;
    }
    return 0.0;
}

static void writeSvgChart(
    const std::vector<Record>& records,
    const fs::path& path,
    const std::string& metric,
    const std::string& title,
    const std::string& yLabel,
    bool logScale
) {
    std::vector<std::string> rawDatasets;
    for (const Record& record : records) {
        if (metric == "quality_ratio" && record.qualityRatio < 0.0) {
            continue;
        }
        if (record.status == "ok" &&
            std::find(rawDatasets.begin(), rawDatasets.end(), record.dataset) == rawDatasets.end()) {
            rawDatasets.push_back(record.dataset);
        }
    }
    const bool aggregateBySource = rawDatasets.size() > 40;

    std::vector<std::string> datasets;
    std::vector<std::string> algorithms;
    std::unordered_map<std::string, double> sums;
    std::unordered_map<std::string, int> counts;
    std::unordered_map<std::string, double> values;

    for (const Record& record : records) {
        if (record.status != "ok") {
            continue;
        }
        if (metric == "quality_ratio" && record.qualityRatio < 0.0) {
            continue;
        }
        const std::string group = aggregateBySource ? record.source : record.dataset;
        if (std::find(datasets.begin(), datasets.end(), group) == datasets.end()) {
            datasets.push_back(group);
        }
        if (std::find(algorithms.begin(), algorithms.end(), record.algorithm) == algorithms.end()) {
            algorithms.push_back(record.algorithm);
        }
        const std::string key = group + "\t" + record.algorithm;
        sums[key] += metricValue(record, metric);
        counts[key] += 1;
    }
    if (datasets.empty() || algorithms.empty()) {
        return;
    }
    for (const auto& entry : sums) {
        values[entry.first] = entry.second / static_cast<double>(counts[entry.first]);
    }

    double maxValue = 1.0;
    for (const auto& entry : values) {
        const double transformed = logScale ? std::log10(entry.second + 1.0) : entry.second;
        maxValue = std::max(maxValue, transformed);
    }

    const int width = std::max(760, 190 * static_cast<int>(datasets.size()));
    const int height = 430;
    const int marginLeft = 70;
    const int marginRight = 24;
    const int marginTop = 52;
    const int marginBottom = 88;
    const double plotWidth = width - marginLeft - marginRight;
    const double plotHeight = height - marginTop - marginBottom;
    const double groupWidth = plotWidth / static_cast<double>(datasets.size());
    const double barWidth = std::min(28.0, groupWidth / static_cast<double>(algorithms.size()) - 8.0);

    const std::unordered_map<std::string, std::string> colors = {
        {"greedy", "#3366cc"},
        {"dynamic_programming", "#dc3912"},
        {"backtracking", "#ff9900"},
        {"branch_bound", "#109618"}
    };

    auto yFor = [&](double value) {
        const double transformed = logScale ? std::log10(value + 1.0) : value;
        return marginTop + plotHeight - transformed / maxValue * plotHeight;
    };

    auto fmt = [&](double value) {
        std::ostringstream out;
        if (metric == "quality_ratio") {
            out << std::fixed << std::setprecision(2) << value;
        } else {
            out << std::fixed << std::setprecision(2) << value;
        }
        return out.str();
    };

    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("cannot write " + path.string());
    }

    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
         << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << ' ' << height << "\">\n";
    file << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    file << "<text x=\"" << width / 2.0 << "\" y=\"28\" text-anchor=\"middle\" "
         << "font-family=\"Arial, sans-serif\" font-size=\"18\" font-weight=\"700\">"
         << htmlEscape(title) << "</text>\n";
    file << "<text x=\"16\" y=\"" << marginTop + plotHeight / 2.0
         << "\" transform=\"rotate(-90 16 " << marginTop + plotHeight / 2.0
         << ")\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" font-size=\"12\">"
         << htmlEscape(yLabel) << "</text>\n";
    file << "<line x1=\"" << marginLeft << "\" y1=\"" << marginTop + plotHeight
         << "\" x2=\"" << width - marginRight << "\" y2=\"" << marginTop + plotHeight
         << "\" stroke=\"#222\"/>\n";
    file << "<line x1=\"" << marginLeft << "\" y1=\"" << marginTop
         << "\" x2=\"" << marginLeft << "\" y2=\"" << marginTop + plotHeight
         << "\" stroke=\"#222\"/>\n";

    for (int tick = 0; tick <= 5; ++tick) {
        const double transformedTick = maxValue * tick / 5.0;
        const double y = marginTop + plotHeight - transformedTick / maxValue * plotHeight;
        const double displayValue = logScale ? std::pow(10.0, transformedTick) - 1.0 : transformedTick;
        file << "<line x1=\"" << marginLeft - 4 << "\" y1=\"" << y
             << "\" x2=\"" << width - marginRight << "\" y2=\"" << y
             << "\" stroke=\"#e6e6e6\"/>\n";
        file << "<text x=\"" << marginLeft - 8 << "\" y=\"" << y + 4
             << "\" text-anchor=\"end\" font-family=\"Arial, sans-serif\" font-size=\"11\" fill=\"#333\">"
             << fmt(displayValue) << "</text>\n";
    }

    for (std::size_t groupIndex = 0; groupIndex < datasets.size(); ++groupIndex) {
        const std::string& dataset = datasets[groupIndex];
        const double groupStart = marginLeft + groupIndex * groupWidth;
        const double groupCenter = groupStart + groupWidth / 2.0;
        file << "<text x=\"" << groupCenter << "\" y=\"" << height - 50
             << "\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" font-size=\"12\">"
             << htmlEscape(dataset) << "</text>\n";

        const double totalBarWidth = algorithms.size() * barWidth + (algorithms.size() - 1) * 6.0;
        double x = groupCenter - totalBarWidth / 2.0;
        for (const auto& algorithm : algorithms) {
            const double value = values[dataset + "\t" + algorithm];
            const double barTop = yFor(value);
            const double barHeight = marginTop + plotHeight - barTop;
            auto colorIt = colors.find(algorithm);
            const std::string color = colorIt == colors.end() ? "#555555" : colorIt->second;
            file << "<rect x=\"" << x << "\" y=\"" << barTop << "\" width=\"" << barWidth
                 << "\" height=\"" << barHeight << "\" fill=\"" << color << "\"/>\n";
            file << "<text x=\"" << x + barWidth / 2.0 << "\" y=\"" << barTop - 5
                 << "\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" font-size=\"10\" fill=\"#333\">"
                 << fmt(value) << "</text>\n";
            x += barWidth + 6.0;
        }
    }

    int legendX = marginLeft;
    const int legendY = height - 24;
    for (const auto& algorithm : algorithms) {
        auto colorIt = colors.find(algorithm);
        const std::string color = colorIt == colors.end() ? "#555555" : colorIt->second;
        std::string label = algorithm;
        std::replace(label.begin(), label.end(), '_', ' ');
        file << "<rect x=\"" << legendX << "\" y=\"" << legendY - 10
             << "\" width=\"12\" height=\"12\" fill=\"" << color << "\"/>\n";
        file << "<text x=\"" << legendX + 18 << "\" y=\"" << legendY
             << "\" font-family=\"Arial, sans-serif\" font-size=\"12\">"
             << htmlEscape(label) << "</text>\n";
        legendX += 160;
    }

    file << "</svg>\n";
}

static void writeOutputs(const std::vector<Record>& records, const fs::path& outDir) {
    fs::create_directories(outDir);
    writeCsv(records, outDir / "results.csv");
    writeJson(records, outDir / "results.json");
    writeSummary(records, outDir / "summary.md");
    writeSvgChart(
        records,
        outDir / "time_ms.svg",
        "time_ms",
        "Running Time by Algorithm",
        "milliseconds (log scale)",
        true
    );
    writeSvgChart(
        records,
        outDir / "estimated_memory_kb.svg",
        "estimated_memory_kb",
        "Estimated Memory by Algorithm",
        "KB",
        false
    );
    writeSvgChart(
        records,
        outDir / "quality_ratio.svg",
        "quality_ratio",
        "Solution Quality by Algorithm",
        "value / reference optimum",
        false
    );
}

static void printTable(const std::vector<Record>& records) {
    std::cout << std::left << std::setw(22) << "source"
              << std::setw(42) << "dataset"
              << std::setw(22) << "algorithm"
              << std::right << std::setw(9) << "value"
              << std::setw(9) << "weight"
              << std::setw(12) << "time_ms"
              << std::setw(12) << "mem_kb"
              << std::setw(10) << "quality" << '\n';
    std::cout << std::string(130, '-') << '\n';

    const std::size_t maxConsoleRows = 200;
    for (std::size_t i = 0; i < records.size() && i < maxConsoleRows; ++i) {
        const Record& record = records[i];
        std::string source = record.source;
        if (source.size() > 20) {
            source = source.substr(0, 19) + "~";
        }
        std::string dataset = record.dataset;
        if (dataset.size() > 40) {
            dataset = dataset.substr(0, 39) + "~";
        }
        std::cout << std::left << std::setw(22) << source
                  << std::setw(42) << dataset
                  << std::setw(22) << record.algorithm;
        if (record.status == "ok") {
            std::cout << std::right << std::setw(9) << formatScaled(record.value, record.valueScale)
                      << std::setw(9) << formatScaled(record.weight, record.weightScale)
                      << std::setw(12) << std::fixed << std::setprecision(4) << record.timeMs
                      << std::setw(12) << std::fixed << std::setprecision(3) << record.memoryKb
                      << std::setw(10) << (record.qualityRatio >= 0.0 ? formatRatio(record.qualityRatio).substr(0, 6) : "-")
                      << '\n';
        } else {
            std::cout << std::right << std::setw(9) << "-"
                      << std::setw(9) << "-"
                      << std::setw(12) << "-"
                      << std::setw(12) << "-"
                      << std::setw(10) << "skipped" << '\n';
        }
    }
    if (records.size() > maxConsoleRows) {
        std::cout << "... " << (records.size() - maxConsoleRows)
                  << " more rows written to the result files\n";
    }
}

static void runSelfTest() {
    Instance instance;
    instance.name = "known_case";
    instance.capacity = 50;
    instance.items = {
        {1, 10, 60},
        {2, 20, 100},
        {3, 30, 120}
    };

    const std::vector<std::string> exactAlgorithms = {
        "dynamic_programming",
        "backtracking",
        "branch_bound"
    };
    for (const auto& algorithm : exactAlgorithms) {
        Solution solution = runAlgorithm(algorithm, instance);
        validateSolution(instance, solution);
        if (solution.value != 220) {
            throw std::runtime_error(algorithm + " self-test failed");
        }
    }

    Solution greedy = solveGreedy(instance);
    validateSolution(instance, greedy);
    if (greedy.value != 160) {
        throw std::runtime_error("greedy self-test failed");
    }
    std::cout << "self-test passed\n";
}

static void printUsage() {
    std::cout
        << "Usage:\n"
        << "  knapsack_experiment [options]\n\n"
        << "Options:\n"
        << "  --self-test                         run built-in correctness test\n"
        << "  --data-dir DIR                      unified/simple dataset directory, default: data\n"
        << "  --uu-dir DIR                        Unicauca raw dataset directory, default: data/_raw/uu\n"
        << "  --jj-root DIR                       JorikJooken raw root, default: data/_raw/jj_extract/knapsackProblemInstances-master\n"
        << "  --data-only                         only read --data-dir, skip legacy raw sources\n"
        << "  --include-raw-sources               also read legacy Unicauca and JorikJooken directories\n"
        << "  --out-dir DIR                       output directory, default: results\n"
        << "  --instance FILE                     run one dataset file, repeatable\n"
        << "  --algorithm NAME                    greedy | dynamic_programming | backtracking | branch_bound | all\n"
        << "  --max-exact-items N                 skip backtracking/branch_bound above N items, default: 30\n"
        << "  --max-dp-states N                   skip DP above n*(capacity+1), default: 50000000\n"
        << "  --help                              show this help\n";
}

int main(int argc, char* argv[]) {
    std::string dataDir = "data";
    std::string uuDir = "data/_raw/uu";
    std::string jjRoot = "data/_raw/jj_extract/knapsackProblemInstances-master";
    std::string outDir = "experiments";
    std::vector<std::string> explicitFiles;
    std::vector<std::string> algorithms = {
        "greedy",
        "dynamic_programming",
        "backtracking",
        "branch_bound"
    };
    int maxExactItems = 30;
    long double maxDpStates = 50000000.0L;
    bool allSources = false;
    bool selfTest = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                printUsage();
                return 0;
            }
            if (arg == "--self-test") {
                selfTest = true;
            } else if (arg == "--data-dir" && i + 1 < argc) {
                dataDir = argv[++i];
            } else if (arg == "--uu-dir" && i + 1 < argc) {
                uuDir = argv[++i];
            } else if (arg == "--jj-root" && i + 1 < argc) {
                jjRoot = argv[++i];
            } else if (arg == "--data-only") {
                allSources = false;
            } else if (arg == "--include-raw-sources") {
                allSources = true;
            } else if (arg == "--out-dir" && i + 1 < argc) {
                outDir = argv[++i];
            } else if (arg == "--instance" && i + 1 < argc) {
                explicitFiles.push_back(argv[++i]);
            } else if (arg == "--algorithm" && i + 1 < argc) {
                const std::string name = argv[++i];
                if (name == "all") {
                    algorithms = {"greedy", "dynamic_programming", "backtracking", "branch_bound"};
                } else if (
                    name == "greedy" ||
                    name == "dynamic_programming" ||
                    name == "backtracking" ||
                    name == "branch_bound"
                ) {
                    if (algorithms.size() == 4 &&
                        algorithms[0] == "greedy" &&
                        algorithms[1] == "dynamic_programming" &&
                        algorithms[2] == "backtracking" &&
                        algorithms[3] == "branch_bound") {
                        algorithms.clear();
                    }
                    algorithms.push_back(name);
                } else {
                    throw std::runtime_error("unknown algorithm: " + name);
                }
            } else if ((arg == "--max-exact-items" || arg == "--max-backtracking-items") && i + 1 < argc) {
                maxExactItems = std::stoi(argv[++i]);
            } else if (arg == "--max-dp-states" && i + 1 < argc) {
                maxDpStates = std::stold(argv[++i]);
            } else {
                throw std::runtime_error("unknown or incomplete option: " + arg);
            }
        }

        if (selfTest) {
            runSelfTest();
            return 0;
        }

        const std::vector<Instance> instances = loadInstances(
            dataDir,
            uuDir,
            jjRoot,
            explicitFiles,
            allSources
        );
        const std::vector<Record> records = runExperiment(
            instances,
            algorithms,
            maxExactItems,
            maxDpStates
        );
        writeOutputs(records, outDir);
        printTable(records);
        std::cout << "\ninstances loaded: " << instances.size() << '\n';
        std::cout << "\noutputs written to: " << fs::absolute(outDir).string() << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
