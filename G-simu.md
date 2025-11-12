# G-simulation Integration Summary

## 新增内容
- `g_simulation_cache_t`（`src/tck-reach/zg-covreach.cc`）  
  - 维护每个离散位置对应的 `G(q)` DBM，初始化为所有未来时钟满足 `x ≤ 0` 的 zone。  
  - 记录每条离散迁移生成的 `prog`（包含源/目标不变量、guard、reset、时间延展许可）。  
  - 通过 `elapsed_semantics_t::prev` + worklist 迭代反向传播，遇到 DBM 扩张触发上游重新检查。
- `node_le_t` 构造函数增设 G-cache 入口，覆盖判定调用 `simulation_leq` 仅对 `G(q)` 收集到的约束进行比较。
- `graph_t::add_edge` 在添加实际边或 subsumption 边时都调用 `record_transition`，保证缓存时时更新。
- `state_space_t` 保存并下发共享的 G-cache，使 `graph_t` 可在节点/边创建阶段即刻登记。

## 变更内容
- `zg-covreach` BFS 主循环保持不变，但覆盖判定逻辑从 `shared_is_le` 转向 G-simulation。  
- `run` 内部构造 zone graph 时禁用 LU extrapolation (`NO_EXTRAPOLATION`)，以保证 DBM 与缓存同步。  
- 头文件 (`src/tck-reach/zg-covreach.hh`) 对应增加 cache 前向声明、构造参数、访问器以及自定义 `add_edge`。

## 移除/替代
- 旧的 LU-based 覆盖剪枝仍保留在代码库，但在默认 `zg-covreach` 流程中不再使用；覆盖判定完全由新的 G-simulation 驱动。  
- 未再调用 `dbm::extra_lu`；相关逻辑通过禁用 extrapolation 规避。

## 验证
- 本地执行 `cmake --build build`，核心目标（`tck-reach` 等）成功生成，仅存在上游 Boost/macOS 版本差异的链接警告。  
- 尚未附加运行时用例，建议后续针对含/不含对角约束的模型做差分测试。

## 对角线用例与测试
| 示例 | 特性 | 测试命令 | 结果要点 |
| ---- | ---- | -------- | -------- |
| `examples/diag_handshake.tck` | 握手超时 (guard 中 `tx-rx≤3`) | `build/src/tck-reach -a covreach examples/diag_handshake.tck` | `REACHABLE=false`, `VISITED_STATES=3` |
| `examples/diag_invariant.tck` | 位于不变式的差分约束 `x-y≤2` | `build/src/tck-reach -a covreach examples/diag_invariant.tck` | `REACHABLE=false`, `VISITED_STATES=5` |
| `examples/diag_rendezvous.tck` | 双进程 rendezvous，guard 含跨进程差分 | `build/src/tck-reach -a covreach examples/diag_rendezvous.tck` | `REACHABLE=false`, `VISITED_STATES=13` |
| `examples/diag_safe_pair.tck` | 人工安全案例，`alarm` 需 `x-y≥3`（不可达） | `build/src/tck-reach -a covreach -l error examples/diag_safe_pair.tck` | `REACHABLE=false`, `STORED_STATES=3` |
| `examples/diag_fail_path.tck` | 故障必达，`fail` guard `x-y≥3` 可触发 | `build/src/tck-reach -a covreach -l error examples/diag_fail_path.tck` | `REACHABLE=true`, `VISITED_TRANSITIONS=10` |

> 上述所有命令均在启用 G-simulation (无 extrapolation) 的 `covreach` 版本下顺利完成，表明该实现正确处理含对角线约束的模型并能区分错误状态的可达/不可达场景。

## 复杂度分析（G(q) 计算与模拟判定）
- **回传 `G(q)`**  
  每条离散迁移在 `record_transition` 中生成一份程序描述（guard / reset / invariant），`apply_program` 通过 `elapsed_semantics_t::prev` 回算前驱 zone，并对 DBM 执行 `tighten`。  
  - `prev`：顺序执行约束求交、reset、时间开张；主要成本来自 `tighten` 的 Floyd-Warshall，复杂度 `O(d^3)`（`d = |X| + 1`）。  
  - `widen`：逐元素取 `max`，`O(d^2)`；若发生变化，再 `tighten` 一次 `O(d^3)`。  
  - Worklist：最坏情况下需遍历所有边 `(q,a,prog,q')` 多次，整体近似 `O(|Δ| · d^3 · iter)`；仿真终止性依赖差分束缚良序，实际迭代数通常受限。

- **模拟判定 `simulation_leq`**  
  针对同一离散位置的两个 zone，逐条检查 `G(q)` 中所有有限约束：  
  - 遍历 `_dim^2` 个 DBM 项，忽略 `LT_INFINITY`。  
  - 每个比较为常数时间，因而一次判定成本 `O(d^2)`。  
  - 若尚未缓存 `G(q)`，退化为原先的 `zone <=`，仍为 `O(d^2)`。

- **总体影响**  
  传统 extrapolation 以 `O(d^2)` 控制 DBM 条目数量；改用 G-simulation 后，额外付出的 `O(d^3)` 回算代价换取对角约束的精确性。对中小规模模型，该成本可以接受，但在大量时钟/复杂拓扑上可能成为瓶颈，需结合实际模型评估。***
## 最新 simulation-relation 代码解读
- **总体思路**  
  - 引入全局 G-simulation 缓存，在同一离散位置上保存一份可以复用的 `G(q)` DBM，上层覆盖算法不再单纯依靠 `zone <=`，而是根据缓存中持有的有限约束比对候选节点的 zone。  
  - 缓存通过记录每条边的 guard/reset/invariant 信息，并在有新约束时以 worklist 形式反向传播，逐步逼近 fixpoint。  
  - Zone graph 生成阶段禁用 extrapolation（`NO_EXTRAPOLATION`），确保缓存里的 `G(q)` 与实际探索到的 zone 完全一致。

- **调用链**  
  1. `run`（`src/tck-reach/zg-covreach.cc:383-399`）负责搭建 zone graph，并将共享的 `g_simulation_cache_t` 交给 `state_space_t`。  
  2. `state_space_t` 构造时把缓存指针传入 `ts::state_space_t`，让 `graph_t` 在节点/边创建时就能利用缓存（`src/tck-reach/zg-covreach.cc:342-350`）。  
  3. 探索过程中调用 `graph_t::add_edge`，在把边纳入覆盖图的同时调用 `g_simulation_cache_t::record_transition`，保证缓存及时更新（`src/tck-reach/zg-covreach.cc:265-272`）。  
  4. 覆盖判定落在 `node_le_t::operator()`，若离散部分一致则使用缓存的 `simulation_leq` 判定 zone 是否受已有 G 约束覆盖（`src/tck-reach/zg-covreach.cc:233-248`）。

- **核心类与函数**  
  - `g_simulation_cache_t`（`src/tck-reach/zg-covreach.cc:33-205`）  
    - `ensure_entry`：为给定离散位置初始化 `G(q)` DBM，并设定 `x ≤ 0` 的基础约束。  
    - `simulation_leq`：在缓存存在的前提下仅对 `G(q)` 中有限约束进行比较，实现弱化的覆盖判断。  
    - `record_transition`：把新边封装为 `g_transition_program_t`，插入 `_incoming` 并触发 `process_pending`。  
    - `apply_program` / `widen`：调用 `elapsed_semantics.prev` 反向推演前驱 zone，只要 `G(src)` 有拓宽就进入队列继续迭代。  
    - `process_pending`：worklist 主循环，确保所有受影响的离散位置都完成 fixpoint 更新。
  - `node_le_t`（`src/tck-reach/zg-covreach.cc:233-248`）  
    - 构造函数保存 G-cache 指针；运算符首先检查离散部分相等，再调用缓存判定。  
  - `graph_t`（`src/tck-reach/zg-covreach.hh:122-170`, `src/tck-reach/zg-covreach.cc:256-286`）  
    - 构造函数把缓存传给基类的 subsumption 谓词。  
    - 重载 `add_edge`，把实际添加的边交给缓存，保证覆盖图与 G(q) 同步增长。  
  - `state_space_t` / `run`（`src/tck-reach/zg-covreach.hh:219-245`, `src/tck-reach/zg-covreach.cc:342-399`）  
    - 统一管理缓存生命周期，确保 zone graph、subsumption graph 与 G-simulation 共享同一份状态。  
    - 将 zone graph 构造策略从 `EXTRA_LU_PLUS_LOCAL` 换成 `NO_EXTRAPOLATION`，避免外部剪枝干扰 G-simulation。

- **后续建议**  
  1. 对含对角约束的模型追加回归测试，核对 `simulation_leq` 生成的覆盖关系是否符合预期。  
  2. 若需进一步说明，可在 `simulation_relation.md` 中补充调用链与 worklist 传播流程，便于文档化。  
  3. 评估在时钟数量较多时的性能，并考虑是否需要对 `_incoming` 或 worklist 存储做优化。

## G(q) 计算流程详解
- **输入与输出**  
  - 输入对象：  
    - `G(q')`：例如二维时钟系统中以 `std::vector<db_t>` 表示的 DBM `[0, x, y]`，若目标位置 `q'` 目前收敛到 `0 ≤ x ≤ 3`, `0 ≤ y ≤ 4`, `x - y ≤ 1`，则对应的矩阵条目分别记录 `x0 ≤ 3`, `0x ≤ 0`, `xy ≤ 1`, `yx ≤ 4` 等上界。  
    - `prog`：`g_transition_program_t` 结构，字段包括 `src_vloc = q`, `vedge = e`, `src_invariant`, `guard`, `reset`, `tgt_invariant` 等。例如 guard `x - y ≥ 1` 被编码为 `clock_constraint_t{ y, x, constraint ≤ -1 }`，reset `x := 0` 展开为 `clock_reset_t{ x, 0 }`。  
  - 输出对象：  
    - 更新后的 `G(q)`：同样是一份 DBM。若通过本次计算得到 `x = 0`, `-1 ≤ x - y ≤ 2`, `y ≤ 0`，矩阵中会体现 `0x = 0`, `x0 = 0`, `xy ≤ 2`, `yx ≤ 1`（对应 `-1 ≤ x-y`），`y0 ≤ 0` 等条目，并在 `tighten` 后存回 `_entries[q]`。

- **计算逻辑**  
  1. `record_transition` 收到新边后把 `prog` 加入 `_incoming[q']`，随即调用 `apply_program`。  
  2. `apply_program` 复制 `G(q')` 的 DBM 到局部变量 `candidate`，调用 `_semantics.prev` 求前驱：  
     - 先将 `candidate` 与 `prog.tgt_invariant` 相交；若 `tgt_delay_allowed=false` 会禁止时间延展。  
     - 逆向执行 reset（把被 reset 的钟回写到源侧）；  
     - 加入 guard；  
     - 再结合源不变量与 `prog.src_delay_allowed` 约束，完成一次 Floyd-Warshall `tighten`。  
  3. 如 `candidate` 非空，即表示存在满足 guard/reset 后可到达 `q'` 的时钟赋值；`widen` 将 `candidate` 的每个界与当前 `G(q)` 取 `max`。  
  4. 若 `G(q)` 界发生变化，`enqueue(q)` 把源位置加入待处理队列，并继续在 `process_pending` 中对 `q` 的所有入边迭代执行上述步骤，直至全局无新增约束。

- **示例**  
  假设有两个时钟 `x`、`y`，离散位置 `q` 含不变量 `x - y ≤ 2`，存在一条迁移到 `q'`，guard 为 `x - y ≥ 1`，reset 把 `x` 置零：
  1. 初始时 `G(q)`、`G(q')` 均为默认的 `x ≤ 0`, `y ≤ 0`。  
  2. 当探索到某个 `q'` 的 zone（例如 `0 ≤ x ≤ 3`, `0 ≤ y ≤ 4`, `x - y ≤ 1`）并添加到覆盖图后，`G(q')` 至少包含这些约束；`record_transition` 会把迁移记入 `_incoming[q']`。  
  3. `apply_program` 在 `prev` 中：  
     - 与 `q'` 不变量相交（无变化）；  
     - 由于 reset(`x`)，将 `x` 的所有出入边界替换为初值 `0`；  
     - 加入 guard `x - y ≥ 1` 的逆向写法（即 `y - x ≤ -1`）；  
     - 与 `q` 的不变量 `x - y ≤ 2` 合并后 `tighten`。  
     最终得到 `candidate` 包含 `-1 ≤ x - y ≤ 2` 与 `x = 0` 等界。  
  4. `widen` 将 `candidate` 并入 `G(q)`，形成新的覆盖范围：`x = 0`、`-1 ≤ x - y ≤ 2`、`y ≤ 0`。  
  5. 若 `q` 还有其他出边，则 `process_pending` 会继续以更新后的 `G(q)` 推动前驱位置的 G 集合扩张，直至所有关联位置稳定。

- **要点**  
  - `G(q)` 只记录那些在工作表中出现过的迁移所能反向推得的时钟约束，因此比直接存储全部 zone 更加紧凑。  
  - 多条迁移指向同一目标位置时，`_incoming[q']` 存储全部 `prog`，Worklist 会自动处理由不同 guard/reset 带来的约束组合。  
  - 当某个位置后续再发现更大的 zone 时（`G(q')` 拓宽），同一算法会再次运行并自动把影响传回 `q`，不需要额外触发点。
