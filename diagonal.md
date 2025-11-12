## 5.1节内容概述：Concrete Simulation Relation for GTA

**在论文的第 5.1 节，“A concrete simulation relation for GTA” 主要定义了一种****具体（concrete）层面的模拟关系**，用于刻画 Generalized Timed Automata (GTA) 之间的行为等价（simulation）。其目标是在时钟约束（clock constraints）以及相应的时间演化下建立状态间的可模拟性。

**核心思想是：**

> **给定两个状态 **`(l₁, v₁)` 与 `(l₂, v₂)`，若 `(l₁, v₁)` 可以模拟 `(l₂, v₂)`，则在所有时钟的推进、guard 检查和 reset 操作下，前者的可达集合应当包含后者的可达集合。

**这种关系最终支撑了后续的 ****zone graph / DBM 操作**。

---

## 二、对角线约束（Diagonal Constraints）在模拟关系中的影响

**论文中提到 GTA 支持的约束形式为：** ** [** ** x\_i - x\_j \\leq c** ** ]** ** 这类约束称为****对角线约束（diagonal constraints）**。与传统的非对角（单钟）约束 (x\_i \\leq c) 或 (x\_i \\geq c) 不同，对角线约束引入了变量间的相关性，使得状态空间的几何结构不再是简单的超矩形，而是一个多维凸多面体。

### 在 simulation relation 中的处理要点：

1. **对角线约束会破坏单钟可分性** ** 传统的 LU-bounds 抽象假设每个时钟独立有上下界 L(x), U(x)。** ** 而在存在约束 **`x - y ≤ c` 时，x 与 y 之间的差成为关键变量，因此简单的 per-clock bound 不再足够表示所有可达区间。
2. **模拟关系的判定** ** 为了在 concrete 层面定义“一个 zone 是否模拟另一个 zone”，论文引入的关系必须考虑所有 **`(x, y)` 对的约束传播。也就是说：** ** [** ** \\forall (x, y): (x - y)^{A₁} \\leq (x - y)^{A₂}** ** ]** ** 这样的比较才是“tight simulation”的基础。

---

## 三、结合你的实现思路：如何在代码层面实现对角约束支持

**你的实现计划与论文的理论处理非常契合，可以具体对应如下：**

### 1. 扩展 clock-bounds 求解（对应论文中 DBM 间的比较）

**论文中提到对角约束是通过 DBM（Difference Bound Matrix）来表示和操作的。** ** 你提出的思路——用加权图 / Floyd–Warshall 求全源最短路径来提取最紧约束——正是 DBM 的经典算法基础。**

**即：** ** [** ** D**[x](#) = \\min(D[x](#), D[x](#) + D[z](#))** ** ]** ** 从而获得所有隐式对角线约束。

> **✅ 对应论文：在构建 simulation relation 时要求闭包（closure）保持所有对角约束的 tightness。** ** ✅ 对应实现：你提到的“为对角约束实现新的传播逻辑”即为 DBM 闭包算法的核心。**

---

### 2. 调整 extrapolation 策略（对应论文中的抽象/近似）

**论文提到在有 diagonal constraints 时，某些抽象（如 LU extrapolation）****不再 sound**。** ** 因为 LU 仅基于单时钟界，而 diagonal constraints 需要跨时钟的上下界。

**你的思路中提到：**

> **“当检测到对角约束时，退化到更保守的抽象（NO\_EXTRAPOLATION 或 EXTRA\_M）。”**

**这与论文建议的策略完全一致：** ** 当无法保证 LU soundness 时，退化为不抽象或仅保持 M-bound。**

> **✅ 对应论文：5.1 中提到的 concrete relation 不再可 factorize，因此抽象层面必须保守。** ** ✅ 对应实现：保守退化策略可确保 soundness，牺牲部分性能。**

---

### 3. Guard访问与重置传播（对应论文中的 simulation step）

**论文中的 simulation step 包括：**

* **检查 guard 是否满足；**
* **执行 reset；**
* **推进时间演化。**

**对角约束使得 reset 不再只影响单时钟，而可能传播到相关时钟。** ** 例如：**

```
x := 0,  约束为  x - y ≤ c
→ 必须更新所有含 x 的差分约束。
```

**你的思路中提出：**

> **“在重置/同步传播时也要考虑 x := y + c 搭配对角 guard 的组合。”**

**这实际上就是 ****在 DBM 上执行 reset 操作时的矩阵更新规则**，类似：** ** [** ** D'[x](#) = c + D[y₀](#) ** ]**

> **✅ 对应论文：simulation relation 在定义 successor relation 时要求考虑 reset 的传播。** ** ✅ 对应实现：在 **`visit(typed_diagonal_clkconstr_expression_t)` 中处理即可。

---

### 4. 诊断与回退机制（ta::has\_diagonal\_constraint）

**论文并未禁止 diagonal constraints，但指出其分析复杂度高。** ** 因此，你的策略：**

> **“检测后不报错，而是切换到保守求解路径。”** ** 完全符合论文思想：soundness 优先。**

---

### 5. 验证路径（实验验证）

**论文中也在后续章节展示了含对角约束的模型示例。** ** 你提到的：**

> **“提供 x - y < 3 的例子，通过 zg/reach/trace 流水线验证。”**

**这可以作为测试对角约束在 reachability、zone generation、simulation relation 中的正确性验证。**

---

## 四、瓶颈与论文一致的挑战点


| **问题**            | **论文中的对应讨论**                | **你的应对方案**            |
| ------------------- | ----------------------------------- | --------------------------- |
| **LU抽象不再sound** | **“Concrete relation不可分解”**   | **退化到NO\_EXTRAPOLATION** |
| **理论负担高**      | **“对角约束下tight bound推理难”** | **可先提供退化实现**        |
| **性能影响**        | **“复杂度由O(n²)升至O(n³)”**    | **可使用缓存/剪枝优化**     |

---

## 五、小结

**你的设计方案与论文5.1节中提出的****对角约束下的具体模拟关系（concrete simulation relation）**思路高度一致：


| **论文概念**                      | **代码实现方向**                                       |
| --------------------------------- | ------------------------------------------------------ |
| **simulation relation考虑(x−y)** | **clockbounds扩展DBM差分传播**                         |
| **diagonal constraint破坏LU抽象** | **退化抽象（NO\_EXTRAPOLATION）**                      |
| **reset传播跨时钟影响**           | **更新**`visit(typed_diagonal_clkconstr_expression_t)` |
| **soundness优先于性能**           | **has\_diagonal\_constraint → 策略切换**              |

**因此，从理论角度，你的方案是正确的，只要确保：**

* **DBM闭包逻辑覆盖所有 **`(x,y)`；
* **extrapolation 在检测到对角约束时退化；**
* **reset/guard访问逻辑更新。**

# 六伪代码实现

**太好了。我把“5.1 A concrete simulation relation for GTA”的要点拆成****能直接落地的伪代码骨架**，并把它怎么接到你现有框架（`compute_clockbounds`、`clockbounds_t`、extrapolation、guard 访问器等）一一对齐。文中涉及的理论定义/规则我都标注了出处，便于你核对实现。

---

# 一、核心数据结构与基础算子（DBM/加权图）

```
// 统一权重：(<, c) 或 (≤, c)，c ∈ ℤ ∪ {−∞, +∞}
struct W { rel: enum {LT, LE}; val: int_or_inf }  

// DBM：索引域为 X ∪ {0}，矩阵含义是 y - x ⩽ w
struct DBM {
  W m[N+1][N+1]   // 索引0代表常量0钟
  bool standard   // 是否已标准化（GTA版）
  bool normalized // 是否已规范化（全源最短路闭包）
}
```

## 1) 标准化 & 规范化（论文 §6.1 / §6.2）

* **标准化**（standardization）：把与未来钟/历史钟相连的 0→x、x→0 边调整到 GTA 语义允许的形态（含 `<,∞` / `≤,0` / `≤,−∞` 这些“扩展权重”），确保“有意义的负环 ↔ 不可满足”在新代数上成立。
* **规范化**（normalization）：Floyd–Warshall 风格闭包，`m[x][y] = min(m[x][y], m[x][z] + m[z][y])`，并在扩展权重代数下求最短路；之后 `normalized=true`。

```
function Standardize(G: DBM): DBM
  for x in XF: G.m[0][x] = min(G.m[0][x], W(LE, 0))
  for x in XH: G.m[x][0] = min(G.m[x][0], W(LE, 0))
  // 若存在有限 x→y 约束，则强化 y→0 / 0→x 为 <,∞（历史钟）或 ≤,0（未来钟）
  // 细则按论文 standard form 规则设置
  G.standard = true
  return G

function Normalize(G: DBM): DBM
  // Floyd–Warshall over extended weights
  for k in V:
    for i in V:
      for j in V:
        G.m[i][j] = min(G.m[i][j], add(G.m[i][k], G.m[k][j]))
  if hasNegativeCycle(G): infeasible
  G.normalized = true
  return G
```

> **注：加法/比较使用论文定义的“扩展权重代数”（总序与可结合加法）。**

## 2) Zone 基本操作（论文 §6.2 定义28/定理29）

```
function IntersectGuard(G: DBM, g: list of atomic y-x ⩽ c or <:  // 支持对角
  for (y, x, w) in g:
    G.m[x][y] = min(G.m[x][y], w)
  G = Standardize(G)
  G = Normalize(G)
  return G

function ResetHistory(G: DBM, x in XH):
  // 历史钟复位：x:=0
  for y in V: G.m[x][y] = if y==0 then W(LE,0) else W(LE,∞)
  for y in V: G.m[y][x] = if y==0 then W(LE,0) else G.m[y][0] // 论文给出的闭式公式
  return Normalize(G)

function ReleaseFuture(G: DBM, x in XF):
  // 未来钟释放：x ∈ [−∞, 0] 非确定
  for y in V: G.m[x][y] = W(LE,∞)
  for y in V: G.m[y][x] = if y==0 then W(LE,0) else G.m[y][0]
  return Normalize(G)

function TimeElapse(G: DBM):
  // 历史钟：0→x 置为 <,∞ ；未来钟：0→x 置为 ≤,0（不可越0）
  for x in XH: if G.m[0][x] != W(LE,∞): G.m[0][x] = W(LT,∞)
  for x in XF: if G.m[0][x] != W(LE,−∞): G.m[0][x] = W(LE,0)
  return Normalize(G)
```

**这些就是构造后继 zone、执行 guard/重置/时间推进所需的全部原语，复杂度 O(|X|²)。**

---

# 二、5.1 节的“具体模拟关系”如何实现

## 1) G(q) 的静态方程（论文式(4)）

> **为每个控制状态 q 计算一组原子约束 G(q)，满足：** ** [** ** G(q) = {x \\le 0 \\mid x \\in X\_F} ;\\cup; \\bigcup\_{q \\xrightarrow{a,prog} q'} \\mathrm{pre}(prog,, G(q'))** ** ]** ** 其中 **`pre` 是**程序逆向传播**算子（对 guard/复位/释放的符号前向-后向变换），用来保证离散步也被模拟关系保留。

```
function PreOfProg(prog, Gset): set<atomic>
  // 顺序程序 pre(prog1;prog2, G) = pre(prog1, pre(prog2,G))
  if prog is seq(p1,p2): return PreOfProg(p1, PreOfProg(p2, Gset))
  if prog is guard g:    return SplitAtomic(g) ∪ Gset
  if prog is change [R]:
     out = {}
     for φ in Gset:
        // φ = (y - x ⩽ c) 的四种情形
        if x∉R and y∉R: out.add(φ)
        if x∈R and y∉R: out.add( (y - 0 ⩽ c) )     // 变成 y ⩽ c
        if x∉R and y∈R: out.add( (0 - x ⩽ c) )     // 变成 −x ⩽ c
        if x∈R and y∈R: // 同时被改写，信息丢失
           // 空集：该原子对前态无约束
     return out
function Compute_G_sets():
  // 最小不动点，直到收敛
  init G(q) = { x ≤ 0 | x ∈ XF }  for all q
  repeat
    changed = false
    for each edge t = (q --a,prog--> q'):
      Gpre = PreOfProg(prog, G(q'))
      if G(q) := G(q) ∪ Gpre changed: changed = true
  until not changed
```

> **有了 **`G(q)`，我们把**具体模拟**定义为：** ** 对同一控制状态 q，若对**所有** φ∈G(q) 与**所有** δ≥0，`Z1` 的任意 v 满足 `v+δ ⊨ φ ⇒` 在 `Z2` 中存在 v' 使 `v'+δ ⊨ φ`，则 `(q,Z1) ⪯ (q,Z2)`。

## 2) Zone 级模拟检查（可执行、保守充分条件）

**直接按定义量化 δ 检查太难，论文给了可实现的构造（把“对所有 δ”折叠进一个****时间闭包**+**对原子取界**的检查）。工程上可用**充分但紧的**条件：

```
// 计算 Z 对某原子 φ: (y-x ⩽ c 或 < c) 的“最强界”
function TightBound(Z: DBM, y, x): W =
  // 规范化后 DBM.m[x][y] 就是 y - x 的最强上界
  return Z.m[x][y]

// 将“对所有 δ≥0”的要求收敛到一次 TimeElapse
function Simulates(q, Z1, Z2): bool
  G1 = Normalize(TimeElapse(Clone(Z1)))
  G2 = Normalize(TimeElapse(Clone(Z2)))
  for each atomic φ ∈ G(q):
    (y,x, wφ) = φ
    // 充分条件：Z2 对 y-x 的上界 ≤ Z1 的上界（越小越强）
    if TightBound(G2,y,x) > TightBound(G1,y,x): return false
  return true
```

> **直观：**`G(q)` 收集了所有需要在离散步前就“守住”的原子；把两边先做一次 `TimeElapse` 再比“上界”，即可把“∀δ”吸收进去。** ** 这与论文给出的“⪯\_G 作为模拟 + 定理14 保证延时/离散步闭包”一致。

---

# 三、接入 `compute_clockbounds` 与 LU/M 抽象策略

## 1) 扩展 clock-bounds（有对角时走 DBM）

* **若模型或当前 zone ****出现对角原子**（检测 `ta::has_diagonal_constraint()`），则：
  * **不再**尝试直接产出逐钟 L/U；
  * **转为维护****退化的 clockbounds**：`has_LU=false`，但保留 `M`（最大常数）与 DBM（或“差分上界视图”）。
* `compute_clockbounds` 可返回：
  ```
  struct clockbounds_t {
    bool has_LU; map<X,int> L, U;  // 若无对角则填充
    int M;                          // 全局最大常数
    optional<DBM> dbm_view;         // 对角版本的紧表示（只读）
  }
  ```

```
function compute_clockbounds(automaton):
  M = scan_max_constant(automaton)
  if not has_diagonal_constraint(automaton):
     // 原实现：提取每钟 L/U（ExtraLU/ExtraM 需要）
     return infer_per_clock_LU(automaton, M)
  else:
     // 对角：构建“最紧差分图”的只读视图供后续模拟/剪枝用
     Gq = Compute_G_sets()        // §5.1 的静态分析
     return clockbounds_t{ has_LU=false, M=M, dbm_view=null } // LU弃用
```

## 2) Extrapolation 策略切换

* **无对角**：沿用 `ExtraLU` 或 `ExtraM`。
* **有对角**：默认
  * `NO_EXTRAPOLATION`，或
  * `ExtraM`（只凭 M 常数截断），**严禁**在不存在单钟上界时硬套 `ExtraLU`。
* **也可以实现“****混合**”：仅当某个钟在当前 DBM 中确有**单钟**上界 `x-0 ⩽ c` 时，允许对该钟局部 LU 约束，否则退化到 `M`/无抽象。

---

# 四、Guard/Reset 访问器改造（对角表达式）

**在 **`df_solver_expr_updater_t::visit(typed_diagonal_clkconstr_expression_t)`：

```
visit(expr: y_minus_x_rel_c):
  // 1) 解析 y, x, rel(≤/<), c
  w = W(rel, c)
  // 2) 在当前 DBM 上做原子更新
  DBM.m[x][y] = min(DBM.m[x][y], w)
  // 3) 立刻 Standardize + Normalize，确保紧闭包
  Standardize(DBM)
  Normalize(DBM)
```

**对于重置/同步（含 **`x := y + c` 的语义组合），可等价为：

* **先把 **`x` 释放/复位到中性形态；
* **再添加两条边 **`y→x` 与 `0→x`/`x→0` 的推导约束，效果等同“赋值后再闭包”。（论文的 `pre([R],G)` 四分情形给了形式依据）

---

# 五、Zone Graph 枚举与模拟剪枝

```
function Explore():
  init_node = (q0, Normalize(TimeElapse( IntersectGuard(TopZone(), g0 ∧ XF≤0 ∧ XH≥0) )))
  Waiting.push(init_node)
  while Waiting not empty:
    (q,Z) = Waiting.pop()
    Passed.add((q,Z))
    for t=(q --a,prog--> q'):
      Z' = Z
      for step in prog:      // 交错的 guard 与 [R]
        if step is guard g:  Z' = IntersectGuard(Z', g)
        if step is [R]:      Z' = ApplyChange(Z', R)  // 复位/释放组合
      Z' = TimeElapse(Z')
      // 模拟剪枝（使用 §5.1 的 ⪯_{G(q')}）
      keep = true
      for (q',Zold) in Passed∪Waiting:
        if q'==q' and Simulates(q', Z', Zold): keep=false; break
      if keep: Waiting.push((q', Z'))
```

> **正确性由 §5 与定理14/20 支撑；含对角时终止性需要 §7/§8 的†不变量与有限性证明，但工程上即便提前退化抽象也不影响 soundness。**

---

# 六、单元测试与验证建议

1. **DBM对角闭包**

* **初始：**`x, y ∈ XH`，加入 `y - x ≤ 3`，检查闭包后 `m[x][y]` 是否为 `(≤,3)`；
* **加入 **`x ≤ 5`、`0 - y ≤ −2` 后检查派生边是否正确；
* **检查负环与不可满足的一致性（标准化后才判断）。**

1. **G(q) 构造**

* **人工构造小 automaton，手算 **`pre` 逆传播；与代码的 G(q) 比对。

1. **含对角的 reach/zg/trace 流水线**

* **例：**`provided: x - y < 3` 的模型，跑到接受；
* **切换 extrapolation：**`ExtraLU` vs `NO_EXTRAPOLATION`（对角时应自动退化）。

---

# 七、复杂度与注意事项

* **单次 **`Intersect/Reset/Release/TimeElapse`：O(|X|²)；全源闭包仍是 Floyd–Warshall。
* **必须**在对角 guard 更新后做一次 **Standardize → Normalize**，否则负环检测会失真（扩展权重代数差异导致）。
* **未来钟时间推进要保持 **`0→x = ≤,0`，防止越 0。

---

# 八、当前实现状态

* **Clock-bounds 求解退化**：`compute_clockbounds` 先对 `clockbounds_t` 进行 `resize`，若检测到 `ta::has_diagonal_constraint(system)`，便只输出一条 warning 并返回 (`src/clockbounds/solver.cc:441-455`)。LU/M map 仍然存在，但保持 `NO_BOUND` 值以便后续 DBM 直接承载仿真关系。
* **Extrapolation 自动降级**：`extrapolation_factory(extrapolation_type, system)` 在相同检测下会将所有 `EXTRA_*` 类型降级为 `NO_EXTRAPOLATION`，同时发出 warning (`src/zg/extrapolation.cc:200-210`)。用户显式选择 `NO_EXTRAPOLATION` 时则按原行为执行。
* **API 注释同步**：`include/tchecker/clockbounds/solver.hh:266-270` 与 `include/tchecker/zg/extrapolation.hh:472-478` 记录了上述 fallback 语义，方便调用方在含对角 guard 的模型上读取期望的退化行为。
* **Simulation relation 不变**：由于覆盖判定完全依赖 DBM，新的退化路径无需修改 `node_le_t`/`shared_is_le`，即可在 zone graph 中继续使用论文 5.1 的仿真关系。
