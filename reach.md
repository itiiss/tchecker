# TChecker 可达性算法 (tck-reach) 核心逻辑解析

本文档旨在解释 TChecker 中 `tck-reach` 工具背后可达性算法的核心逻辑，特别是其“等待列表”（Waiting List）/“通过列表”（Passed List）的设计，以及关键的“区域模拟关系”（Zone Simulation Relation）检查。

## 1. 概述

TChecker 的覆盖可达性算法 (`covreach`) 是一个经典的图搜索算法。它维护一个“待探索”状态的集合（`waiting` 列表）和一个“已探索”状态的图结构（`passed` 列表的等价物）。其核心思想是：在将一个新计算出的后继状态加入 `waiting` 列表之前，检查这个新状态是否已经被 `passed` 列表中的某个状态所“覆盖”（subsumed）。如果新状态被覆盖，那么就没必要探索它了，从而实现对搜索空间的剪枝。

这个过程的调用链始于 `tck-reach` 工具的特定实现（例如 `zg-covreach`，用于区域图的覆盖可达性），并最终调用一个通用的、模板化的图搜索算法。

## 2. 代码调用链分析

### 2.1. 入口: `zg-covreach.cc`

`tck-reach` 工具针对不同的状态模型有不同的实现。当我们处理带有时钟的区域图（Zone Graph）时，`src/tck-reach/zg-covreach.cc` 文件中的 `run` 函数是分析的起点。

此函数负责：
1.  构建一个区域图 `tchecker::zg::zg_t`。
2.  创建一个 `subsumption::graph_t` 实例，它将存储最终的状态空间图。这个图结构原生支持“覆盖”检查。
3.  实例化一个 `algorithm_t` 对象，并调用其 `run` 方法来启动算法。

```cpp
// in src/tck-reach/zg-covreach.cc

std::tuple<...> run(...)
{
  // ... system and zone graph (zg) setup ...

  // 状态空间图，它是一个 subsumption::graph_t
  std::shared_ptr<tchecker::tck_reach::zg_covreach::state_space_t> state_space = ...;

  // 算法实例
  tchecker::tck_reach::zg_covreach::algorithm_t algorithm;

  // 启动算法
  stats = algorithm.run<...>(state_space->zg(), state_space->graph(), ...);

  return std::make_tuple(stats, state_space);
}
```

### 2.2. 算法特化: `zg-covreach.hh`

`zg-covreach.cc` 中使用的 `algorithm_t` 实际上是一个别名。其定义位于 `src/tck-reach/zg-covreach.hh` 中，它继承自一个通用的模板算法。

```cpp
// in src/tck-reach/zg-covreach.hh

#include "tchecker/algorithms/covreach/algorithm.hh" // <--- 关键的 include

class algorithm_t
    : public tchecker::algorithms::covreach::algorithm_t<tchecker::zg::zg_t, tchecker::tck_reach::zg_covreach::graph_t> {
  // ...
};
```
这清楚地表明，真正的算法实现在 `tchecker/algorithms/covreach/algorithm.hh` 中。

### 2.3. 通用算法核心: `algorithm.hh`

`include/tchecker/algorithms/covreach/algorithm.hh` 文件包含了算法的核心逻辑。这里的 `run` 方法实现了你提到的“探索循环”。

```cpp
// in include/tchecker/algorithms/covreach/algorithm.hh

template <class TS, class GRAPH>
class algorithm_t {
public:
  template <...>
  stats_t run(TS & ts, GRAPH & graph, ...)
  {
    // 1. 初始化 Waiting List
    std::unique_ptr<tchecker::waiting::waiting_t<node_sptr_t>> waiting{...};
    // ...
    expand_initial_nodes(ts, graph, nodes, stats);
    for (node_sptr_t const & n : nodes)
      waiting->insert(n);

    // 2. 核心探索循环
    while (!waiting->empty()) {
      node_sptr_t node = waiting->first();
      waiting->remove_first();

      // ... 检查是否为目标状态 ...

      // 3. 计算后继状态
      expand_next_nodes(node, ts, graph, nodes, stats);

      // 4. 将未被覆盖的后继状态加入 Waiting List
      for (node_sptr_t const & next_node : nodes) {
        waiting->insert(next_node);
        // ...
      }
      nodes.clear();
    }
    // ...
    return stats;
  }
};
```

## 3. 关键：仿真/覆盖检查 (`is_covered`)

覆盖检查发生在 `expand_next_nodes` 函数（以及处理初始状态的 `expand_initial_nodes`）内部。

```cpp
// in include/tchecker/algorithms/covreach/algorithm.hh

void expand_next_nodes(...)
{
  // ...
  ts.next(node->state_ptr(), sst); // 获取所有后继状态
  for (auto && [status, s, t] : sst) {
    // 为后继状态创建一个图节点
    typename GRAPH::node_sptr_t next_node = graph.add_node(s);

    // ***** 这是最关键的检查 *****
    if (graph.is_covered(next_node, covering_node)) {
      // 如果被覆盖，则不加入 waiting list，仅添加一条指向“覆盖者”的“归入边”
      graph.add_edge(node, covering_node, tchecker::graph::subsumption::EDGE_SUBSUMPTION, *t);
      graph.remove_node(next_node); // 丢弃新节点
    }
    else {
      // 如果未被覆盖，则添加一条实际的边，并将其加入 next_nodes 列表
      // （之后 next_nodes 中的节点会被加入 waiting list）
      graph.add_edge(node, next_node, tchecker::graph::subsumption::EDGE_ACTUAL, *t);
      next_nodes.push_back(next_node);
    }
  }
}
```

### 3.1. `is_covered` 如何工作？

`graph.is_covered` 方法会遍历图中已有的、与新节点 `next_node` 具有相同离散部分（即相同的地点组合和整数变量取值）的节点，并对它们进行比较。

这个比较操作最终委托给了 `node_le_t` 类，它在 `zg-covreach.cc` 中定义，是专门为区域图节点定制的比较器。

```cpp
// in src/tck-reach/zg-covreach.cc

bool node_le_t::operator()(tchecker::tck_reach::zg_covreach::node_t const & n1,
                           tchecker::tck_reach::zg_covreach::node_t const & n2) const
{
  // le: Less than or Equal to
  return tchecker::zg::shared_is_le(n1.state(), n2.state());
}
```

### 3.2. 区域模拟关系: `is_le` 的实现

`tchecker::zg::shared_is_le` 函数是“区域模拟关系”的最终实现。对于两个状态（`state1`, `state2`），当且仅当：
1.  它们拥有完全相同的离散部分（自动机位置、整数变量值）。
2.  `state1` 的区域（Zone）被 `state2` 的区域完全包含（`Zone1 ⊆ Zone2`）。

区域（Zone）在 TChecker 中由差分约束矩阵（Difference-Bound Matrix, DBM）表示。**区域 `Z1` 包含于 `Z2` 的条件是，对于 DBM 中的所有条目 `(i, j)`，`DBM1(i, j) <= DBM2(i, j)` 必须成立。**

这个 DBM 级别的比较是剪枝优化的数学基础。如果新状态的区域已经包含在一个已探索的区域之内，那么从新状态出发能够到达的任何行为，也都能从那个已探索的区域出发到达，因此无需再对新状态进行探索。

## 4. 总结

TChecker 的可达性分析流程设计精良，通过模板和回调将通用算法与特定模型分离开来：

1.  **探索循环**：一个通用的 `while (!waiting->empty())` 循环位于 `covreach/algorithm.hh`。
2.  **后继计算**：通过 `ts.next()`（对区域图来说是 `zg.next()`）动态计算。
3.  **Passed List**：由 `subsumption::graph_t` 数据结构隐式管理。
4.  **覆盖检查**：在 `expand_next_nodes` 中，通过 `graph.is_covered()` 调用。
5.  **仿真关系**：检查最终由特定模型的比较器（如 `node_le_t`）实现，它对区域图执行 DBM 级别的包含关系检查，即 `DBM_new(i,j) <= DBM_passed(i,j)`。

这个设计确保了算法的核心逻辑是通用的，同时允许为不同类型的模型（如区域图）插入高度优化的、特定于领域的覆盖（剪枝）策略。
