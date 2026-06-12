// 0/1 背包问题多算法对比实验主程序
// 负责数据集读取、统一调度各算法、结果校验与导出（CSV/JSON/Markdown/SVG）
// 算法实现见 greedy-knapsack01 / dp-knapsack01 / backtracking-knapsack01 / branch_bound-knapsack01
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
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "knapsack01_common.h"
#include "greedy-knapsack01.h"
#include "dp-knapsack01.h"
#include "backtracking-knapsack01.h"
#include "branch_bound-knapsack01.h"

using namespace std;

namespace fs = filesystem;

static const vector<string> kDefaultAlgorithms = {
    "greedy",
    "dynamic_programming",
    "dynamic_programming_2d",
    "backtracking",
    "branch_bound"
};

static const vector<string> kKnownAlgorithms = {
    "greedy",
    "greedy_value",
    "greedy_weight",
    "dynamic_programming",
    "dynamic_programming_2d",
    "backtracking",
    "branch_bound"
};

struct Record {
    string source;
    string dataset;
    int itemCount{};
    ll capacity{};
    ll knownOptimum{-1};
    long double valueScale{1.0L};
    long double weightScale{1.0L};
    string algorithm;
    string status;
    ll value{};
    ll weight{};
    vector<int> selectedItems;
    double timeMs{};
    double memoryKb{};
    bool optimal{};
    ll referenceOptimum{};
    double qualityRatio{-1.0};
    string details;
};

static string trim(const string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return s;
}

static vector<string> splitWords(string line) {
    replace(line.begin(), line.end(), ',', ' ');
    istringstream input(line);
    vector<string> parts;
    string part;
    while (input >> part) {
        parts.push_back(part);
    }
    return parts;
}

static string joinIndexes(const vector<int>& values, const string& sep = " ") {
    ostringstream output;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << sep;
        }
        output << values[i];
    }
    return output.str();
}

static string csvEscape(const string& value) {
    bool needQuotes = value.find_first_of(",\"\n\r") != string::npos;
    if (!needQuotes) {
        return value;
    }

    string escaped = "\"";
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

static string htmlEscape(const string& value) {
    string escaped;
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

static bool startsWith(const string& value, const string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

static vector<ll> readNumbersFromFile(const fs::path& path) {
    ifstream file(path);
    if (!file) {
        throw runtime_error("cannot open file: " + path.string());
    }

    vector<ll> numbers;
    string token;
    while (file >> token) {
        if (!token.empty() && token[0] == '#') {
            string ignored;
            getline(file, ignored);
            continue;
        }
        numbers.push_back(stoll(token));
    }
    return numbers;
}

static vector<string> readNumberTokensFromFile(const fs::path& path) {
    ifstream file(path);
    if (!file) {
        throw runtime_error("cannot open file: " + path.string());
    }

    vector<string> tokens;
    string token;
    while (file >> token) {
        if (!token.empty() && token[0] == '#') {
            string ignored;
            getline(file, ignored);
            continue;
        }
        tokens.push_back(token);
    }
    return tokens;
}

static bool hasDecimalToken(const string& token) {
    return token.find('.') != string::npos ||
           token.find('e') != string::npos ||
           token.find('E') != string::npos;
}

static ll parseScaled(const string& token, long double scale) {
    return static_cast<ll>(llround(stold(token) * scale));
}

static string formatScaled(ll value, long double scale, int precision = 4) {
    if (scale == 1.0L) {
        return to_string(value);
    }

    ostringstream output;
    output << fixed << setprecision(precision)
           << static_cast<long double>(value) / scale;
    string text = output.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

static string formatRatio(double value) {
    if (value < 0.0) {
        return "";
    }
    ostringstream output;
    output << fixed << setprecision(6) << value;
    return output.str();
}

static void validateInstance(const Instance& instance, const fs::path& path) {
    if (instance.capacity < 0) {
        throw runtime_error("capacity must be non-negative in " + path.string());
    }
    if (instance.items.empty()) {
        throw runtime_error("empty instance in " + path.string());
    }
    for (const Item& item : instance.items) {
        if (item.weight <= 0 || item.value < 0) {
            throw runtime_error("invalid item weight/value in " + path.string());
        }
    }
}

static Instance readSimpleInstance(const fs::path& path) {
    ifstream file(path);
    if (!file) {
        throw runtime_error("cannot open dataset: " + path.string());
    }

    Instance instance;
    instance.name = path.stem().string();
    instance.source = "FSU";
    bool hasCapacity = false;
    string capacityToken;
    string knownOptimumToken;
    vector<vector<string>> tokenRows;

    string rawLine;
    while (getline(file, rawLine)) {
        const auto comment = rawLine.find('#');
        if (comment != string::npos) {
            const string commentText = trim(rawLine.substr(comment + 1));
            const string lowerComment = toLower(commentText);
            if (startsWith(lowerComment, "source:")) {
                instance.source = trim(commentText.substr(string("source:").size()));
            } else if (startsWith(lowerComment, "optimal value:")) {
                knownOptimumToken = trim(commentText.substr(string("optimal value:").size()));
            }
            rawLine = rawLine.substr(0, comment);
        }

        const string line = trim(rawLine);
        if (line.empty()) {
            continue;
        }

        const auto parts = splitWords(line);
        if (parts.empty()) {
            continue;
        }

        string key = toLower(parts[0]);
        if (!key.empty() && key.back() == ':') {
            key.pop_back();
        }

        if (key == "name" && parts.size() >= 2) {
            instance.name.clear();
            for (size_t i = 1; i < parts.size(); ++i) {
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
                size_t parsed = 0;
                stold(part, &parsed);
                if (parsed != part.size()) {
                    throw invalid_argument("trailing characters");
                }
            }
        } catch (const exception&) {
            throw runtime_error("cannot parse line in " + path.string() + ": " + line);
        }
        tokenRows.push_back(parts);
    }

    vector<vector<string>> itemRows;
    if (!hasCapacity) {
        if (tokenRows.empty() || tokenRows[0].size() < 2) {
            throw runtime_error("missing header n capacity in " + path.string());
        }
        const ll declaredN = stoll(tokenRows[0][0]);
        capacityToken = tokenRows[0][1];
        itemRows.assign(tokenRows.begin() + 1, tokenRows.end());
        if (declaredN != static_cast<ll>(itemRows.size())) {
            throw runtime_error("item count mismatch in " + path.string());
        }
    } else {
        itemRows = tokenRows;
    }

    bool hasDecimalWeight = hasDecimalToken(capacityToken);
    bool hasDecimalValue = hasDecimalToken(knownOptimumToken);
    for (const auto& row : itemRows) {
        if (row.size() < 2) {
            throw runtime_error("item row must contain weight and value in " + path.string());
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

    for (size_t i = 0; i < itemRows.size(); ++i) {
        Item item;
        item.index = static_cast<int>(i + 1);
        item.weight = parseScaled(itemRows[i][0], instance.weightScale);
        item.value = parseScaled(itemRows[i][1], instance.valueScale);
        instance.items.push_back(item);
    }

    if (instance.source.find("Florida State University") != string::npos) {
        instance.source = "FSU KNAPSACK_01";
    }
    validateInstance(instance, path);
    return instance;
}

static Instance readUnicaucaInstance(const fs::path& path, const fs::path& optimumPath) {
    const vector<string> tokens = readNumberTokensFromFile(path);
    if (tokens.size() < 2) {
        throw runtime_error("invalid Unicauca instance: " + path.string());
    }

    Instance instance;
    instance.name = path.filename().string();
    instance.source = startsWith(path.parent_path().filename().string(), "large")
        ? "Unicauca large_scale"
        : "Unicauca low-dimensional";

    const ll declaredN = stoll(tokens[0]);
    const size_t itemTokenCount = static_cast<size_t>(2 + declaredN * 2);
    const size_t itemAndSelectionTokenCount = static_cast<size_t>(2 + declaredN * 3);
    if (tokens.size() != itemTokenCount && tokens.size() != itemAndSelectionTokenCount) {
        throw runtime_error("item count mismatch in " + path.string());
    }

    bool hasDecimal = false;
    for (size_t i = 1; i < tokens.size(); ++i) {
        hasDecimal = hasDecimal || hasDecimalToken(tokens[i]);
    }
    if (hasDecimal) {
        instance.valueScale = 1000000.0L;
        instance.weightScale = 1000000.0L;
    }

    instance.capacity = parseScaled(tokens[1], instance.weightScale);
    for (ll i = 0; i < declaredN; ++i) {
        const ll value = parseScaled(tokens[static_cast<size_t>(2 + i * 2)], instance.valueScale);
        const ll weight = parseScaled(tokens[static_cast<size_t>(2 + i * 2 + 1)], instance.weightScale);
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
    const unordered_map<string, ll>& optima
) {
    const vector<ll> numbers = readNumbersFromFile(testPath);
    if (numbers.size() < 2) {
        throw runtime_error("invalid JorikJooken instance: " + testPath.string());
    }

    Instance instance;
    instance.name = testPath.parent_path().filename().string();
    instance.source = "JorikJooken";

    const ll declaredN = numbers[0];
    if (numbers.size() != static_cast<size_t>(1 + declaredN * 3 + 1)) {
        throw runtime_error("item count mismatch in " + testPath.string());
    }

    for (ll i = 0; i < declaredN; ++i) {
        const size_t offset = static_cast<size_t>(1 + i * 3);
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

static Solution runAlgorithm(const string& name, const Instance& instance) {
    if (name == "greedy") {
        return solveGreedy(instance);
    }
    if (name == "greedy_value") {
        return solveGreedy(instance, "value");
    }
    if (name == "greedy_weight") {
        return solveGreedy(instance, "weight");
    }
    if (name == "dynamic_programming") {
        return solveDpRolling(instance);
    }
    if (name == "dynamic_programming_2d") {
        return solveDp2d(instance);
    }
    if (name == "backtracking") {
        return solveBacktracking(instance);
    }
    if (name == "branch_bound") {
        return solveBranchBound(instance);
    }
    throw runtime_error("unknown algorithm: " + name);
}

static Record makeSkippedRecord(
    const Instance& instance,
    const string& algorithm,
    const string& reason
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
    record.optimal = !startsWith(algorithm, "greedy");
    record.details = reason;
    return record;
}

static Record runOne(const Instance& instance, const string& algorithm) {
    const auto start = chrono::high_resolution_clock::now();
    Solution solution = runAlgorithm(algorithm, instance);
    const auto finish = chrono::high_resolution_clock::now();
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
    record.timeMs = chrono::duration<double, milli>(finish - start).count();
    record.memoryKb = static_cast<double>(solution.memoryBytes) / 1024.0;
    record.optimal = solution.optimal;
    record.details = solution.details;
    return record;
}

static unordered_map<string, ll> loadJjOptima(const fs::path& optimaPath) {
    unordered_map<string, ll> optima;
    ifstream file(optimaPath);
    if (!file) {
        return optima;
    }

    string line;
    getline(file, line);
    while (getline(file, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto comma = line.find(',');
        if (comma == string::npos) {
            continue;
        }
        const string name = trim(line.substr(0, comma));
        const ll value = stoll(trim(line.substr(comma + 1)));
        optima[name] = value;
    }
    return optima;
}

static void appendSimpleInstances(vector<Instance>& instances, const fs::path& dataDir) {
    if (!fs::exists(dataDir)) {
        return;
    }

    vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            paths.push_back(entry.path());
        }
    }
    sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        instances.push_back(readSimpleInstance(path));
    }
}

static void appendUnicaucaGroup(
    vector<Instance>& instances,
    const fs::path& instanceDir,
    const fs::path& optimumDir
) {
    if (!fs::exists(instanceDir)) {
        return;
    }

    vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(instanceDir)) {
        if (entry.is_regular_file()) {
            paths.push_back(entry.path());
        }
    }
    sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        instances.push_back(readUnicaucaInstance(path, optimumDir / path.filename()));
    }
}

static void appendUnicaucaInstances(vector<Instance>& instances, const fs::path& uuDir) {
    appendUnicaucaGroup(instances, uuDir / "low-dimensional", uuDir / "low-dimensional-optimum");
    appendUnicaucaGroup(instances, uuDir / "large_scale", uuDir / "large_scale-optimum");
}

static void appendJjInstances(vector<Instance>& instances, const fs::path& jjRoot) {
    const fs::path problemDir = jjRoot / "problemInstances";
    if (!fs::exists(problemDir)) {
        return;
    }

    const auto optima = loadJjOptima(jjRoot / "optima.csv");
    vector<fs::path> paths;
    for (const auto& entry : fs::recursive_directory_iterator(problemDir)) {
        if (entry.is_regular_file() && entry.path().filename() == "test.in") {
            paths.push_back(entry.path());
        }
    }
    sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        instances.push_back(readJjInstance(path, optima));
    }
}

static vector<Instance> loadInstances(
    const string& dataDir,
    const string& uuDir,
    const string& jjRoot,
    const vector<string>& explicitFiles,
    bool allSources
) {
    vector<Instance> instances;

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
        throw runtime_error("no dataset files found");
    }
    return instances;
}

static vector<Record> runExperiment(
    const vector<Instance>& instances,
    const vector<string>& algorithms,
    int maxExactItems,
    long double maxDpStates
) {
    vector<Record> records;

    for (const Instance& instance : instances) {
        vector<size_t> rowIndexes;

        for (const auto& algorithm : algorithms) {
            rowIndexes.push_back(records.size());
            if (startsWith(algorithm, "dynamic_programming")) {
                const long double states = static_cast<long double>(instance.items.size()) *
                    static_cast<long double>(instance.capacity + 1);
                if (states > maxDpStates) {
                    records.push_back(makeSkippedRecord(
                        instance,
                        algorithm,
                        "states>" + to_string(static_cast<ll>(maxDpStates)) +
                            "; use --max-dp-states to run"
                    ));
                    continue;
                }
                // 二维表每状态 8 字节，超过 2GB 直接跳过，避免分配失败
                if (algorithm == "dynamic_programming_2d" &&
                    states * 8.0L > 2000000000.0L) {
                    records.push_back(makeSkippedRecord(
                        instance,
                        algorithm,
                        "2d table>2GB; use dynamic_programming (rolling) instead"
                    ));
                    continue;
                }
            }

            if ((algorithm == "backtracking" || algorithm == "branch_bound") &&
                static_cast<int>(instance.items.size()) > maxExactItems) {
                records.push_back(makeSkippedRecord(
                    instance,
                    algorithm,
                    "n>" + to_string(maxExactItems) +
                        "; use --max-exact-items to run"
                ));
                continue;
            }
            try {
                records.push_back(runOne(instance, algorithm));
            } catch (const exception& ex) {
                Record failed = makeSkippedRecord(
                    instance,
                    algorithm,
                    string("failed: ") + ex.what()
                );
                failed.status = "failed";
                records.push_back(failed);
            }
        }

        ll reference = instance.knownOptimum > 0 ? instance.knownOptimum : 0;
        for (size_t index : rowIndexes) {
            const Record& record = records[index];
            if (record.status == "ok" && record.optimal) {
                reference = max(reference, record.value);
            }
        }

        for (size_t index : rowIndexes) {
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

static void writeCsv(const vector<Record>& records, const fs::path& path) {
    ofstream file(path);
    if (!file) {
        throw runtime_error("cannot write " + path.string());
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
                 << fixed << setprecision(4) << record.timeMs << ','
                 << fixed << setprecision(3) << record.memoryKb << ','
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

static string jsonString(const string& value) {
    string result = "\"";
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

static void writeJson(const vector<Record>& records, const fs::path& path) {
    ofstream file(path);
    if (!file) {
        throw runtime_error("cannot write " + path.string());
    }

    file << "[\n";
    for (size_t i = 0; i < records.size(); ++i) {
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
        for (size_t j = 0; j < record.selectedItems.size(); ++j) {
            if (j > 0) {
                file << ", ";
            }
            file << record.selectedItems[j];
        }
        file << "],\n";
        file << "    \"time_ms\": " << fixed << setprecision(4) << record.timeMs << ",\n";
        file << "    \"estimated_memory_kb\": " << fixed << setprecision(3) << record.memoryKb << ",\n";
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

static void writeSummary(const vector<Record>& records, const fs::path& path) {
    ofstream file(path);
    if (!file) {
        throw runtime_error("cannot write " + path.string());
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
                 << " | " << fixed << setprecision(4) << record.timeMs
                 << " | " << fixed << setprecision(3) << record.memoryKb
                 << " | " << formatRatio(record.qualityRatio)
                 << " | " << joinIndexes(record.selectedItems)
                 << " |\n";
        } else {
            file << " | | | | | skipped: " << record.details << " |\n";
        }
    }
}

static double metricValue(const Record& record, const string& metric) {
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
    const vector<Record>& records,
    const fs::path& path,
    const string& metric,
    const string& title,
    const string& yLabel,
    bool logScale
) {
    vector<string> rawDatasets;
    for (const Record& record : records) {
        if (metric == "quality_ratio" && record.qualityRatio < 0.0) {
            continue;
        }
        if (record.status == "ok" &&
            find(rawDatasets.begin(), rawDatasets.end(), record.dataset) == rawDatasets.end()) {
            rawDatasets.push_back(record.dataset);
        }
    }
    const bool aggregateBySource = rawDatasets.size() > 40;

    vector<string> datasets;
    vector<string> algorithms;
    unordered_map<string, double> sums;
    unordered_map<string, int> counts;
    unordered_map<string, double> values;

    for (const Record& record : records) {
        if (record.status != "ok") {
            continue;
        }
        if (metric == "quality_ratio" && record.qualityRatio < 0.0) {
            continue;
        }
        const string group = aggregateBySource ? record.source : record.dataset;
        if (find(datasets.begin(), datasets.end(), group) == datasets.end()) {
            datasets.push_back(group);
        }
        if (find(algorithms.begin(), algorithms.end(), record.algorithm) == algorithms.end()) {
            algorithms.push_back(record.algorithm);
        }
        const string key = group + "\t" + record.algorithm;
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
        const double transformed = logScale ? log10(entry.second + 1.0) : entry.second;
        maxValue = max(maxValue, transformed);
    }

    const int width = max(
        max(760, 190 * static_cast<int>(datasets.size())),
        160 + 175 * static_cast<int>(algorithms.size())
    );
    const int height = 430;
    const int marginLeft = 70;
    const int marginRight = 24;
    const int marginTop = 52;
    const int marginBottom = 88;
    const double plotWidth = width - marginLeft - marginRight;
    const double plotHeight = height - marginTop - marginBottom;
    const double groupWidth = plotWidth / static_cast<double>(datasets.size());
    const double barWidth = min(28.0, groupWidth / static_cast<double>(algorithms.size()) - 8.0);

    const unordered_map<string, string> colors = {
        {"greedy", "#3366cc"},
        {"greedy_value", "#0099c6"},
        {"greedy_weight", "#dd4477"},
        {"dynamic_programming", "#dc3912"},
        {"dynamic_programming_2d", "#990099"},
        {"backtracking", "#ff9900"},
        {"branch_bound", "#109618"}
    };

    auto yFor = [&](double value) {
        const double transformed = logScale ? log10(value + 1.0) : value;
        return marginTop + plotHeight - transformed / maxValue * plotHeight;
    };

    auto fmt = [&](double value) {
        ostringstream out;
        out << fixed << setprecision(2) << value;
        return out.str();
    };

    ofstream file(path);
    if (!file) {
        throw runtime_error("cannot write " + path.string());
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
        const double displayValue = logScale ? pow(10.0, transformedTick) - 1.0 : transformedTick;
        file << "<line x1=\"" << marginLeft - 4 << "\" y1=\"" << y
             << "\" x2=\"" << width - marginRight << "\" y2=\"" << y
             << "\" stroke=\"#e6e6e6\"/>\n";
        file << "<text x=\"" << marginLeft - 8 << "\" y=\"" << y + 4
             << "\" text-anchor=\"end\" font-family=\"Arial, sans-serif\" font-size=\"11\" fill=\"#333\">"
             << fmt(displayValue) << "</text>\n";
    }

    for (size_t groupIndex = 0; groupIndex < datasets.size(); ++groupIndex) {
        const string& dataset = datasets[groupIndex];
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
            const string color = colorIt == colors.end() ? "#555555" : colorIt->second;
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
        const string color = colorIt == colors.end() ? "#555555" : colorIt->second;
        string label = algorithm;
        replace(label.begin(), label.end(), '_', ' ');
        file << "<rect x=\"" << legendX << "\" y=\"" << legendY - 10
             << "\" width=\"12\" height=\"12\" fill=\"" << color << "\"/>\n";
        file << "<text x=\"" << legendX + 18 << "\" y=\"" << legendY
             << "\" font-family=\"Arial, sans-serif\" font-size=\"12\">"
             << htmlEscape(label) << "</text>\n";
        legendX += 175;
    }

    file << "</svg>\n";
}

static void writeOutputs(const vector<Record>& records, const fs::path& outDir) {
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

static void printTable(const vector<Record>& records) {
    cout << left << setw(22) << "source"
              << setw(42) << "dataset"
              << setw(26) << "algorithm"
              << right << setw(9) << "value"
              << setw(9) << "weight"
              << setw(12) << "time_ms"
              << setw(12) << "mem_kb"
              << setw(10) << "quality" << '\n';
    cout << string(142, '-') << '\n';

    const size_t maxConsoleRows = 200;
    for (size_t i = 0; i < records.size() && i < maxConsoleRows; ++i) {
        const Record& record = records[i];
        string source = record.source;
        if (source.size() > 20) {
            source = source.substr(0, 19) + "~";
        }
        string dataset = record.dataset;
        if (dataset.size() > 40) {
            dataset = dataset.substr(0, 39) + "~";
        }
        cout << left << setw(22) << source
                  << setw(42) << dataset
                  << setw(26) << record.algorithm;
        if (record.status == "ok") {
            cout << right << setw(9) << formatScaled(record.value, record.valueScale)
                      << setw(9) << formatScaled(record.weight, record.weightScale)
                      << setw(12) << fixed << setprecision(4) << record.timeMs
                      << setw(12) << fixed << setprecision(3) << record.memoryKb
                      << setw(10) << (record.qualityRatio >= 0.0 ? formatRatio(record.qualityRatio).substr(0, 6) : "-")
                      << '\n';
        } else {
            cout << right << setw(9) << "-"
                      << setw(9) << "-"
                      << setw(12) << "-"
                      << setw(12) << "-"
                      << setw(10) << "skipped" << '\n';
        }
    }
    if (records.size() > maxConsoleRows) {
        cout << "... " << (records.size() - maxConsoleRows)
                  << " more rows written to the result files\n";
    }
}

static void runSelfTest() {
    // 用例 1：教材经典实例，最优值 220
    Instance instance;
    instance.name = "known_case";
    instance.capacity = 50;
    instance.items = {
        {1, 10, 60},
        {2, 20, 100},
        {3, 30, 120}
    };

    const vector<string> exactAlgorithms = {
        "dynamic_programming",
        "dynamic_programming_2d",
        "backtracking",
        "branch_bound"
    };
    for (const auto& algorithm : exactAlgorithms) {
        Solution solution = runAlgorithm(algorithm, instance);
        validateSolution(instance, solution);
        if (solution.value != 220) {
            throw runtime_error(algorithm + " self-test failed");
        }
    }

    // 贪心三种准则：密度准则 160，价值准则恰好 220，重量准则 160
    Solution greedy = runAlgorithm("greedy", instance);
    validateSolution(instance, greedy);
    if (greedy.value != 160) {
        throw runtime_error("greedy self-test failed");
    }
    Solution greedyValue = runAlgorithm("greedy_value", instance);
    validateSolution(instance, greedyValue);
    if (greedyValue.value != 220) {
        throw runtime_error("greedy_value self-test failed");
    }
    Solution greedyWeight = runAlgorithm("greedy_weight", instance);
    validateSolution(instance, greedyWeight);
    if (greedyWeight.value != 160) {
        throw runtime_error("greedy_weight self-test failed");
    }

    // 用例 2：固定的 12 件物品实例，交叉验证四种精确算法结果一致
    Instance cross;
    cross.name = "cross_check";
    cross.capacity = 87;
    cross.items = {
        {1, 23, 92}, {2, 31, 57}, {3, 29, 49}, {4, 44, 68},
        {5, 53, 60}, {6, 38, 43}, {7, 63, 67}, {8, 85, 84},
        {9, 89, 87}, {10, 82, 72}, {11, 7, 30}, {12, 17, 55}
    };

    ll expected = -1;
    for (const auto& algorithm : exactAlgorithms) {
        Solution solution = runAlgorithm(algorithm, cross);
        validateSolution(cross, solution);
        if (expected < 0) {
            expected = solution.value;
        } else if (solution.value != expected) {
            throw runtime_error(algorithm + " cross-check failed: " +
                                to_string(solution.value) + " != " + to_string(expected));
        }
    }

    // 贪心解不应超过精确最优值
    for (const string& name : {string("greedy"), string("greedy_value"), string("greedy_weight")}) {
        Solution solution = runAlgorithm(name, cross);
        validateSolution(cross, solution);
        if (solution.value > expected) {
            throw runtime_error(name + " exceeds exact optimum in cross-check");
        }
    }

    cout << "self-test passed\n";
}

static void printUsage() {
    cout
        << "Usage:\n"
        << "  knapsack01_experiment [options]\n\n"
        << "Options:\n"
        << "  --self-test                         run built-in correctness test\n"
        << "  --data-dir DIR                      unified/simple dataset directory, default: data\n"
        << "  --uu-dir DIR                        Unicauca raw dataset directory, default: data/_raw/uu\n"
        << "  --jj-root DIR                       JorikJooken raw root, default: data/_raw/jj_extract/knapsackProblemInstances-master\n"
        << "  --data-only                         only read --data-dir, skip legacy raw sources\n"
        << "  --include-raw-sources               also read legacy Unicauca and JorikJooken directories\n"
        << "  --out-dir DIR                       output directory, default: experiments\n"
        << "  --instance FILE                     run one dataset file, repeatable\n"
        << "  --algorithm NAME                    greedy | greedy_value | greedy_weight |\n"
        << "                                      dynamic_programming (1d rolling) |\n"
        << "                                      dynamic_programming_2d | backtracking |\n"
        << "                                      branch_bound | all, repeatable\n"
        << "  --max-exact-items N                 skip backtracking/branch_bound above N items, default: 30\n"
        << "  --max-dp-states N                   skip DP above n*(capacity+1), default: 50000000\n"
        << "  --help                              show this help\n";
}

int main(int argc, char* argv[]) {
    string dataDir = "data";
    string uuDir = "data/_raw/uu";
    string jjRoot = "data/_raw/jj_extract/knapsackProblemInstances-master";
    string outDir = "experiments";
    vector<string> explicitFiles;
    vector<string> algorithms = kDefaultAlgorithms;
    bool algorithmsCustomized = false;
    int maxExactItems = 30;
    long double maxDpStates = 50000000.0L;
    bool allSources = false;
    bool selfTest = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const string arg = argv[i];
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
                const string name = argv[++i];
                if (name == "all") {
                    algorithms = kDefaultAlgorithms;
                    algorithmsCustomized = false;
                } else if (
                    find(kKnownAlgorithms.begin(), kKnownAlgorithms.end(), name) !=
                    kKnownAlgorithms.end()
                ) {
                    if (!algorithmsCustomized) {
                        algorithms.clear();
                        algorithmsCustomized = true;
                    }
                    if (find(algorithms.begin(), algorithms.end(), name) == algorithms.end()) {
                        algorithms.push_back(name);
                    }
                } else {
                    throw runtime_error("unknown algorithm: " + name);
                }
            } else if ((arg == "--max-exact-items" || arg == "--max-backtracking-items") && i + 1 < argc) {
                maxExactItems = stoi(argv[++i]);
            } else if (arg == "--max-dp-states" && i + 1 < argc) {
                maxDpStates = stold(argv[++i]);
            } else {
                throw runtime_error("unknown or incomplete option: " + arg);
            }
        }

        if (selfTest) {
            runSelfTest();
            return 0;
        }

        const vector<Instance> instances = loadInstances(
            dataDir,
            uuDir,
            jjRoot,
            explicitFiles,
            allSources
        );
        const vector<Record> records = runExperiment(
            instances,
            algorithms,
            maxExactItems,
            maxDpStates
        );
        writeOutputs(records, outDir);
        printTable(records);
        cout << "\ninstances loaded: " << instances.size() << '\n';
        cout << "\noutputs written to: " << fs::absolute(outDir).string() << '\n';
    } catch (const exception& ex) {
        cerr << "error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
