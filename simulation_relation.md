# 仿真关系实现对照

## 5.1 节仿真关系的代码对应
- **同一控制点筛选**：`node_hash_t` 只哈希离散部分 `tchecker::ta::shared_hash_value`，保证仅比较同一控制点及整型变量估值的结点 (`src/tck-reach/zg-covreach.cc:37`).
- **预序判定**：`node_le_t` 调用 `tchecker::zg::shared_is_le`，其先比对离散部分，再调用 `zone() <=` 做 zone 包含性检查 (`src/tck-reach/zg-covreach.cc:46`, `src/zg/state.cc:53`).
- **G(q) 的具体化**：zone 自身就是一组 DBM 约束，来源于初始零区、位置不变式、guard 以及 reset 的闭包。`elapsed_semantics_t::next` 中按“约束→reset→时间延展”顺序叠加这些原子约束 (`src/zg/semantics.cc:131-155`)，等价于论文中对 `pre(prog, G(q'))` 的构造（在无时钟赋值的设定下，`tchecker::dbm::reset` 退化为常见的 0/常数重置 `src/dbm/dbm.cc:330-347`).
- **δ≥0 的要求**：`elapsed_semantics` 会在目标可延时位置使用 `open_up` 展开未来时间 (`src/zg/semantics.cc:148-152`)，保证 zone 中的估值包含所有满足 δ 延时的解，从而与定义中“若 v+δ ⊨ φ 则 v'+δ ⊨ φ”保持一致。

## Z ⪯_{G(q)} Z' 的实现细节
- **Zone 包含性**：`shared_is_le` 调用 `zone::operator<=` (`src/zg/state.cc:53`, `src/zg/zone.cc:45-53`)，后者落到 DBM 按元素比较 (`src/dbm/dbm.cc:315-327`)，等价于检查所有原子约束是否在被覆盖 zone 中保持（论文的逐约束验证）。
- **同维度及空区处理**：实现先处理维度与空区特殊情形 (`src/zg/zone.cc:47-52`)，对应论文中对不可满足 zone 的 vacuous 成立情况。

## ¬(Z ⪯_{G(q)} Z') 的判定
- **否定测试**：`graph.is_covered` 在同一哈希桶内遍历候选结点，只要 `node_le_t` 返回 `false` 就认定未覆盖 (`include/tchecker/graph/cover_graph.hh:70-101`). 这正是老师强调的 `¬(Z_i ⪯_{G(q)} Z_j)` 情形：当 `dbm::is_le` 发现任一条约束更严格（`DBM1(i,j) > DBM2(i,j)`）便立即返回 `false` (`src/dbm/dbm.cc:323-327`)，从而拒绝 subsumption 并保留新结点。

## 终止性（第 8 节）对应
- **良性序的实现**：`covreach` 扩展结点后调用 `remove_covered_nodes`，把被新结点覆盖的旧结点删除 (`include/tchecker/algorithms/covreach/algorithm.hh:129-166`). 该函数通过 `covered_nodes` 查找所有被包含的 zone，并把其入边改为对新结点的 subsumption 边。由于 DBM 包含关系在同一控制点上构成良序，重复移除覆盖项确保探索不会在同一位置生成无限严格下降链，对应论文第 8 节的终止性证明。

## 小结
综上，论文 5.1 节预序 `⪯_G` 在代码中表现为：
1. 通过哈希筛选同一控制点；
2. 利用 zone（DBM）封装 `G(q)` 中的原子约束；
3. 用 `dbm::is_le` 执行约束逐项比较；
4. 在覆盖失败时保留新 zone，成功时删旧保新，借由良性序实现第 8 节的覆盖终止性保证。

## 自然语言解读
在覆盖算法里，工具会记住每个控制点配上一块“时间区域”（zone）。当新的区域出现时，我们先确认它指向同一个控制点，再检查老区域里的所有约束对新区域是否仍然成立。如果成立，就表示老区域的行为已经被新区域“模拟”，可以删掉老区域；否则就保留。这样做的好处是一方面能避免无限增加越来越“紧”的区域，另一方面也确保不会丢掉可能到达接受位置的行为，从而既保证终止又保持正确性。
