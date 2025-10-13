# Clock Difference Constraints

## Minimal Experiment
```
system:clock_diff_min

event:tau

process:P
clock:1:x
clock:1:y
location:P:l0{initial:}
location:P:l1{}
edge:P:l0:l1:tau{provided:x-y<3}
```
Running `build/src/tck-reach -a reach examples/clock_diff_min.tck` aborts with:
```
ERROR: unsupported diagonal constraints
```
which shows the shipped reachability binary rejects guards of the form `x - y < 3`.

## Source Code Evidence
- `src/clockbounds/solver.cc:366` handles simple constraints, while `src/clockbounds/solver.cc:388` immediately throws `"unsupported diagonal constraints"` on `typed_diagonal_clkconstr_expression_t`, so the solver that feeds the LU bounds never accepts diagonal guards.
- `include/tchecker/clockbounds/solver.hh:41` documents `df_solver_t` as a "Clock bounds solver for diagonal free timed automata", formally restricting the algorithms that rely on it to diagonal-free models.
- `src/tck-reach/zg-reach.cc:179` constructs the zone graph via `tchecker::zg::factory(..., tchecker::zg::EXTRA_LU_PLUS_LOCAL, ...)`, which calls `tchecker::clockbounds::compute_clockbounds` (see `src/zg/extrapolation.cc:228`). Because this computation delegates to the diagonal-free solver above, any diagonal constraint triggers the runtime error observed.
- `src/ta/static_analysis.cc:37` includes a helper `has_diagonal_constraint` used across the code base to detect such guards, reinforcing that diagonals are treated as a special case that many analyses exclude.

## Conclusion
Both the minimal run and the referenced source confirm that the current reachability tooling in this tree does **not** support diagonal clock constraints like `x - y < 3`; the underlying clock-bounds solver is explicitly restricted to diagonal-free timed automata and aborts as soon as such a constraint appears.
