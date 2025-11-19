# 本次提交改动文件一览

## 文档
- `G-simu.md`：新增 G-simulation 集成说明，梳理缓存结构、状态空间改造、测试结论及复杂度分析。
- `dia_test.md`：整理每个对角线示例的运行命令与期望结果，便于回归验证。
- `diagonal.md`：对论文中 5.1 节的仿真关系与对角约束效应做中文解读，并结合实现要点。
- `simulation.tex`：补充一份 LaTeX 报告，系统阐述 zone 仿真链路、复杂度与源码位置。
- `simulation_relation.md`：逐条映射论文定义与当前代码，实现细节及终止性保证都有对应说明。

## 示例模型
- `examples/diag_dual_watchdog.tck`：新增双看门狗漂移示例，利用 `xb-xa≥4` 的差分 guard 触发错误。
- `examples/diag_dual_watchdog_safe.tck`：在上述模型基础上引入 `xb-xa<3` 的不变式，形成安全对照组。
- `examples/diag_fail_path.tck`：构造两个时钟的漂移路径，`y-x<=-3` 触发 `fail`，验证含负差分的可达性。
- `examples/diag_handshake.tck`：建模发送/应答超时流程，`tx-rx≤3` 的对角 guard 检验仿真覆盖。
- `examples/diag_invariant.tck`：在位置不变式中加入 `x-y<=2`，测试 G(q) 对对角不变式的处理。
- `examples/diag_rendezvous.tck`：包含两个进程的 rendezvous 模型，跨进程差分 `cs-cr<=1` 用于检查同步。
- `examples/diag_safe_pair.tck`：安全配对场景，`y-x<=-3` 的 alarm guard 验证错误不可达。
- `examples/no_diag_simple.tck`：提供无对角约束的 baseline 模型，方便对比新旧策略。

## 代码
- `include/tchecker/algorithms/covreach/algorithm.hh`：给 `expand_next_nodes` 增加中文注释，解释覆盖判定流程。
- `include/tchecker/clockbounds/solver.hh`：更新接口注释，说明检测到对角约束时仅初始化 LU/M 映射而不再抛异常。
- `include/tchecker/graph/cover_graph.hh`：补充 `is_covered` 的说明，明确遍历策略与输出含义。
- `include/tchecker/zg/extrapolation.hh`：文档化 diagonal 情况下自动降级为 `NO_EXTRAPOLATION` 的行为。
- `src/clockbounds/solver.cc`：实现对角约束检测；当存在 diagonal guard/invariant 时只调整尺寸并发出 warning，而不是继续求解 LU。
- `src/dbm/dbm.cc`：为 `is_le` 添加用途、参数和机制注释，强调其在仿真链路中的角色。
- `src/tck-reach/zg-covreach.cc`：重写覆盖逻辑，引入 `g_simulation_cache_t` 维护各位置的 G(q) DBM、记录转移并反向传播；`node_le_t`/`graph_t`/`state_space_t` 支持该缓存，且默认禁用 LU extrapolation 以保持仿真精度。
- `src/tck-reach/zg-covreach.hh`：声明并注入 G-cache，重载 `add_edge` 以便在建图时同步缓存，同时扩展 `node_le_t` 构造与访问器。
- `src/zg/extrapolation.cc`：在工厂中检测 diagonal 约束，打印 warning 并改用 `no_extrapolation_t`；补充相关头文件与日志依赖。
- `src/zg/semantics.cc`：为 `elapsed_semantics_t::prev` 补全逐步注释，解释逆向约束流程。
- `src/zg/state.cc`：扩展 `shared_is_le` 的注释，说明仿真判定的离散/连续条件。
- `src/zg/zone.cc`：为 zone 包含运算提供注释，强调其与 DBM 比较的一一对应逻辑。
