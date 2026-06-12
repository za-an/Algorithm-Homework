# 0/1 背包算法对比实验

本项目围绕经典 0/1 背包问题，实现并比较贪心、动态规划、回溯和分支限界四类算法。程序可批量读取统一格式数据集，自动完成求解、计时、结果校验，并导出 CSV、JSON、Markdown 摘要和 SVG 图表。

## 项目概览

| 内容 | 说明 |
| --- | --- |
| 问题 | 0/1 背包问题 |
| 语言 | C++17 |
| 数据规模 | `data/` 中共 3279 个统一格式实例 |
| 默认算法 | `greedy`、`dynamic_programming`、`dynamic_programming_2d`、`backtracking`、`branch_bound` |
| 输出目录 | `experiments/` |
| 报告材料 | `reports/实验报告.md`、`reports/讲解PPT.pptx`、`reports/讲解视频.mp4` |

## 快速开始

编译：

```powershell
g++ -std=c++17 -O2 -Wall -Wextra `
  .\src\knapsack01_common.cpp `
  .\src\greedy-knapsack01.cpp `
  .\src\dp-knapsack01.cpp `
  .\src\backtracking-knapsack01.cpp `
  .\src\branch_bound-knapsack01.cpp `
  .\src\knapsack01_experiment.cpp `
  -o .\src\knapsack01_experiment.exe
```

运行内置正确性测试：

```powershell
.\src\knapsack01_experiment.exe --self-test
```

运行默认实验：

```powershell
.\src\knapsack01_experiment.exe
```

运行单个实例：

```powershell
.\src\knapsack01_experiment.exe --instance .\data\knapsack01-fsu-01.txt
```

只运行指定算法：

```powershell
.\src\knapsack01_experiment.exe --algorithm greedy --algorithm dynamic_programming
```

## 目录结构

```text
.
├─ data/          统一格式的 0/1 背包数据集
├─ experiments/   程序导出的实验结果和图表
├─ reports/       实验报告、PPT、讲解视频等材料
├─ src/           C++ 源码与实验主程序
├─ member.txt     小组成员与分工
└─ README.md
```

## 算法实现

| 模块 | 作用 |
| --- | --- |
| `knapsack01_common.h/.cpp` | 公共数据结构、价值密度排序、分数背包上界、解校验 |
| `greedy-knapsack01.h/.cpp` | 贪心近似算法，支持按密度、价值、重量三种准则选择 |
| `dp-knapsack01.h/.cpp` | 动态规划精确算法，包含二维表和一维滚动数组版本 |
| `backtracking-knapsack01.h/.cpp` | 回溯精确算法，使用分数背包上界剪枝 |
| `branch_bound-knapsack01.h/.cpp` | 分支限界精确算法，使用优先队列进行最佳优先搜索 |
| `knapsack01_experiment.cpp` | 数据读取、算法调度、计时、校验和结果导出 |

## 数据集

`data/` 目录保存了已经统一格式化的正式数据文件：

| 数据来源 | 文件范围 | 数量 |
| --- | --- | ---: |
| FSU KNAPSACK_01 | `knapsack01-fsu-01.txt` 至 `knapsack01-fsu-08.txt` | 8 |
| Unicauca low-dimensional | `knapsack01-unicauca-low-dimensional-01.txt` 至 `knapsack01-unicauca-low-dimensional-10.txt` | 10 |
| Unicauca large_scale | `knapsack01-unicauca-large-scale-01.txt` 至 `knapsack01-unicauca-large-scale-21.txt` | 21 |
| JorikJooken knapsackProblemInstances | `knapsack01-jorikjooken-0001.txt` 至 `knapsack01-jorikjooken-3240.txt` | 3240 |

统一输入格式：

```text
n capacity
weight_1 value_1
weight_2 value_2
...
weight_n value_n
```

以 `#` 开头的注释行会被忽略，可用于记录来源、实例名称和已知最优值。

## 常用参数

| 参数 | 说明 |
| --- | --- |
| `--self-test` | 运行内置正确性测试 |
| `--data-dir DIR` | 指定统一格式数据目录，默认 `data` |
| `--out-dir DIR` | 指定结果输出目录，默认 `experiments` |
| `--instance FILE` | 指定单个实例文件，可重复传入 |
| `--algorithm NAME` | 指定算法，可重复传入；`all` 表示默认算法集合 |
| `--max-exact-items N` | 超过 N 个物品时跳过回溯和分支限界，默认 30 |
| `--max-dp-states N` | 超过 `n * (capacity + 1)` 状态数时跳过动态规划，默认 50000000 |
| `--include-raw-sources` | 额外读取旧版原始 Unicauca 与 JorikJooken 数据目录 |
| `--data-only` | 只读取 `--data-dir` 指定的统一格式目录 |

支持的算法名称：

```text
greedy
greedy_value
greedy_weight
dynamic_programming
dynamic_programming_2d
backtracking
branch_bound
all
```

## 实验输出

默认结果写入 `experiments/`：

| 文件 | 内容 |
| --- | --- |
| `results.csv` | 主实验表格，含实例信息、算法、状态、解、耗时、内存估算和质量比 |
| `results.json` | 与 CSV 对应的结构化结果 |
| `summary.md` | Markdown 摘要表 |
| `time_ms.svg` | 各算法运行时间对比 |
| `estimated_memory_kb.svg` | 各算法估算内存对比 |
| `quality_ratio.svg` | 各算法解质量比对比 |

## 小组分工

| 成员 | 分工 |
| --- | --- |
| Xu Xinghan | 动态规划、贪心 |
| Ma Ning | 回溯法 |
| Zhang Jie | 分支限界法 |
