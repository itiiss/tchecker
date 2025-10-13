# TChecker Commands Usage

## 1. tck-syntax: 语法检查和模型转换

```
Usage: build/src/tck-syntax [options] [file]
   --asynchronous-events  reports all asynchronous events in the model
   -c                     syntax check (timed automaton)
   -p                     synchronized product
   -t                     transform a system into dot graphviz file format
   -j                     transform a system into json file format
   -o file                output file
   -d delim               delimiter string (default: _)
   -n name                name of synchronized process (default: P)
   --only-processes       only output processes in dot graphviz format(combined with -t)
   -h                     help
reads from standard input if file is not provided
```

Example:

```
./src/tck-syntax -c ../fisher.tck
```

## 2. tck-reach: 可达性分析

```
Usage: build/src/tck-reach [options] [file]
   -a algorithm  reachability algorithm
          reach          standard reachability algorithm over the zone graph
          concur19       reachability algorithm over the local-time zone graph, with sync-subsumption
          covreach       reachability algorithm over the zone graph with inclusion subsumption
          aLU-covreach   reachability algorithm over the zone graph with aLU subsumption
   -C type       type of certificate
          none       no certificate (default)
          graph      graph of explored state-space
          symbolic   symbolic run to a state with searched labels if any
          concrete   concrete run to a state with searched labels if any (only for reach and covreach)
   -h            help
   -l l1,l2,...  comma-separated list of searched labels
   -o out_file   output file for certificate (default is standard output)
   -s bfs|dfs    search order
   --block-size  size of allocation blocks
   --table-size  size of hash tables
reads from standard input if file is not provided
```

Example:

```
./src/tck-reach -a reach -s bfs ../fisher.tck
```

## 3. tck-liveness: 活性分析

```
Usage: build/src/tck-liveness [options] [file]
   -a algorithm  liveness algorithm
          couvscc    Couvreur's SCC-decomposition-based algorithm
                     search an accepting cycle that visits all labels
          ndfs       nested depth-first search algorithm over the zone graph
                     search an accepting cycle with a state with all labels
   -C type       type of certificate
          none       no certificate (default)
          graph      graph of explored state-space
          symbolic   symbolic lasso run with loop on labels (not for couvscc with multiple labels)
   -h            help
   -l l1,l2,...  comma-separated list of accepting labels
   -o out_file   output file for certificate (default is standard output)
   --block-size  size of allocation blocks
   --table-size  size of hash tables
reads from standard input if file is not provided
```

Example:

```
./src/tck-liveness -a ndfs -l accepting ../fisher.tck
```

## 4. tck-simulate: 模拟执行

```
Usage: build/src/tck-simulate [options] [file]
   -1          one-step simulation (output initial or next states if combined with -s)
   -i          interactive simulation (default)
   -r N        randomized simulation, N steps
   -o file     output file for simulation trace (default: stdout)
   -t          output simulation trace, incompatible with -1
   -h          help
reads from standard input if file is not provided
```

Example:

```
./src/tck-simulate -r 10 ../fisher.tck
```

## 5. tck-matrix: 区域矩阵可视化

```
Usage: build/src/tck-matrix [options] [file]
   -h                     help
   -o out_file            output file (default is standard output)
   -i                     show initial zone matrix only
   -d                     detailed output (both constraints and matrix)
   -s simulation_steps    perform simulation with given steps and show zone matrices
reads from standard input if file is not provided
```

Example:

```
./src/tck-matrix -s 5 ../fisher.tck 
```
