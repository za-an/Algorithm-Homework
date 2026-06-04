# 代码文件说明

`src/knapsack01_experiment.cpp`

- 功能：实现 0/1 背包问题实验程序，包含贪心算法、动态规划、回溯法、分支限界法、数据读取、结果校验和结果导出。
- 编译：

```powershell
g++ -std=c++17 -O2 -Wall -Wextra .\src\knapsack01_experiment.cpp -o .\src\knapsack01_experiment.exe
```

- 运行：

```powershell
.\src\knapsack01_experiment.exe
```

- 输入参数：
  - `--self-test`：运行内置正确性测试。
  - `--data-dir DIR`：指定统一格式数据目录，默认 `data`。
  - `--out-dir DIR`：指定实验结果输出目录，默认 `experiments`。
  - `--instance FILE`：指定单个数据文件，可重复传入。
  - `--algorithm NAME`：指定算法，支持 `greedy`、`dynamic_programming`、`backtracking`、`branch_bound`、`all`。
  - `--max-exact-items N`：回溯法和分支限界法最大求解物品数，默认 30。
  - `--max-dp-states N`：动态规划最大状态数，默认 50000000。

- 输入格式：数据文件第一行为 `n capacity`，之后每行为 `weight value`。注释行以 `#` 开头，可包含数据来源和已知最优值。
- 输出文件：默认写入 `experiments/results.csv`、`experiments/results.json`、`experiments/summary.md`、`experiments/time_ms.svg`、`experiments/estimated_memory_kb.svg`、`experiments/quality_ratio.svg`。

# 数据文件说明

数据文件位于 `data/`，均为统一格式：第一行 `n capacity`，之后每行 `weight value`。目录下共有 3279 个正式数据文件。

- `knapsack01-fsu-01.txt` 至 `knapsack01-fsu-08.txt`：来源为 Florida State University KNAPSACK_01，共 8 个实例。
- `knapsack01-unicauca-low-dimensional-01.txt` 至 `knapsack01-unicauca-low-dimensional-10.txt`：来源为 Unicauca low-dimensional，共 10 个实例。
- `knapsack01-unicauca-large-scale-01.txt` 至 `knapsack01-unicauca-large-scale-21.txt`：来源为 Unicauca large_scale，共 21 个实例。
- `knapsack01-jorikjooken-0001.txt` 至 `knapsack01-jorikjooken-3240.txt`：来源为 JorikJooken knapsackProblemInstances，共 3240 个实例。

提交目录仅保留统一格式后的正式数据文件。

# 结果文件说明

实验结果默认位于 `experiments/`。

- `results.csv`：主实验表格，包含数据来源、数据集、物品数量、容量、算法、状态、求解价值、总重量、选择物品编号、运行时间、估算内存、参考最优值和质量比。
- `results.json`：与 CSV 对应的结构化 JSON 结果。
- `summary.md`：Markdown 摘要表。
- `time_ms.svg`：不同算法运行时间图，单位毫秒。
- `estimated_memory_kb.svg`：不同算法估算内存图，单位 KB。
- `quality_ratio.svg`：求解质量比图，质量比为算法结果值除以参考最优值。

默认参数：

- 应用程序名称：`knapsack01_experiment.exe`
- 调用入口：`main`
- 算法集合：`greedy`、`dynamic_programming`、`backtracking`、`branch_bound`
- 回溯法/分支限界法阈值：`--max-exact-items 30`
- 动态规划状态阈值：`--max-dp-states 50000000`
