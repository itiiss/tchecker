

## Dual Watchdog Drift Escalation

File: `examples/diag_dual_watchdog.tck`

```tck
system:diag_dual_watchdog

event:beatA
event:beatB
event:missB
event:alarm
event:recover

process:Monitor
clock:1:xa
clock:1:xb
location:Monitor:aligned{initial: : invariant: xa<=5 && xb<=5}
location:Monitor:stagger{invariant: xa<=7 && xb<=7 }
location:Monitor:hazard{labels: error}
edge:Monitor:aligned:aligned:beatA{provided: xa>=2 : do: xa=0}
edge:Monitor:aligned:aligned:beatB{provided: xb>=4 : do: xb=0}
edge:Monitor:aligned:stagger:missB{provided: xb-xa>=4 && xb>=4 && xa<=1}
edge:Monitor:aligned:aligned:recover{provided: xa<=1 && xb<=1 : do: xa=0; xb=0}
edge:Monitor:stagger:stagger:beatA{provided: xa>=2 : do: xa=0}
edge:Monitor:stagger:stagger:beatB{provided: xb>=4 : do: xb=0}
edge:Monitor:stagger:hazard:alarm{provided: xb-xa>=4}
edge:Monitor:stagger:aligned:recover{provided: xa<=1 && xb<=1 : do: xa=0; xb=0}
```

**场景含义**  
- `xa`/`xb` 分别记录主、备控制器上一次心跳的时间，系统在 `aligned` 中保持两者的节奏。  
- `beatA` 和 `beatB` 代表及时心跳，`missB` 捕获“备份心跳滞后且主控刚刷新”的窗口（`xb-xa ≥ 4` 与 `xa ≤ 1` 联合出现），系统随即进入告警监控态 `stagger`。  
- 若滞后继续扩大到 4 时间单位以上（`xb-xa ≥ 4`），`alarm` 事件将系统送入带 `error` 标签的 `hazard`，表示双冗余被判定失效。  
- 只要两次心跳在 1 时间单位内重新对齐（`recover` 事件），即可回到 `aligned`。

## Reachability Run

Command:

```sh
 ./build/src/tck-reach -a covreach -l error -C concrete examples/diag_dual_watchdog.tck
```

Output:

```
COVERED_STATES 16
MEMORY_MAX_RSS 9617408
REACHABLE true
RUNNING_TIME_SECONDS 0.00129933
STORED_STATES 7
VISITED_STATES 9
VISITED_TRANSITIONS 22
```

> Note: the warning is expected—it indicates the tool switched to the conservative `NO_EXTRAPOLATION` mode introduced for diagonal support. All runs complete successfully without the previous “unsupported diagonal constraints” abort.

---

## Dual Watchdog With Bounded Drift (Safe)

File: `examples/diag_dual_watchdog_safe.tck`

```tck
system:diag_dual_watchdog

event:beatA
event:beatB
event:missB
event:alarm
event:recover

process:Monitor
clock:1:xa
clock:1:xb
location:Monitor:aligned{initial: : invariant: xa<=5 && xb<=5}
location:Monitor:stagger{invariant: xa<=7 && xb<=7 && xb-xa < 3 }
location:Monitor:hazard{labels: error}
edge:Monitor:aligned:aligned:beatA{provided: xa>=2 : do: xa=0}
edge:Monitor:aligned:aligned:beatB{provided: xb>=4 : do: xb=0}
edge:Monitor:aligned:stagger:missB{provided: xb-xa>=4 && xb>=4 && xa<=1}
edge:Monitor:aligned:aligned:recover{provided: xa<=1 && xb<=1 : do: xa=0; xb=0}
edge:Monitor:stagger:stagger:beatA{provided: xa>=2 : do: xa=0}
edge:Monitor:stagger:stagger:beatB{provided: xb>=4 : do: xb=0}
edge:Monitor:stagger:hazard:alarm{provided: xb-xa>=4}
edge:Monitor:stagger:aligned:recover{provided: xa<=1 && xb<=1 : do: xa=0; xb=0}
```

**场景含义**  
- 与上一节相同的双冗余心跳监控，但 `stagger` 中加入额外不变量 `xb-xa <= 3`，表明在告警态下运维逻辑会强制补齐备份延迟。  
- `missB` 仍可触发短暂滞后的 `stagger`；然而因为 `xb-xa` 最多保持 3，`alarm` 的 guard 永远不会满足。  
- `recover` 允许在两次心跳重新对齐时返回 `aligned`，保证系统可持续运行。

## Reachability Run

Command:

```sh
./build/src/tck-reach -a covreach -l error -C concrete examples/diag_dual_watchdog_safe.tck
```

Output:

```
COVERED_STATES 25
MEMORY_MAX_RSS 9551872
REACHABLE false
RUNNING_TIME_SECONDS 0.00192046
STORED_STATES 4
VISITED_STATES 10
VISITED_TRANSITIONS 28
```
