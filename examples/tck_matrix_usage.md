# TChecker Zone Matrix Visualization Tool (tck-matrix) Usage Guide

## Overview

`tck-matrix` is a specialized command-line tool for visualizing zones as Difference Bound Matrix (DBM) representations. It loads TChecker model files and displays zone matrices with proper clock labeling, making it invaluable for understanding and debugging zone-based verification algorithms.

## Command Syntax

```bash
tck-matrix [options] [model_file]
```

## Command Options

- `-h` : Display help information
- `-o <file>` : Output to specified file (default: stdout)
- `-i` : Display initial zone matrix only
- `-d` : Detailed output (constraints + matrix format)
- `-s <steps>` : Perform simulation and show zone evolution

## Basic Usage Examples

### Example 1: Display Initial Zone Matrix

```bash
./tck-matrix -i model.tck
```

**Output:**
```
=== TChecker Zone Matrix Visualization ===
System: timer_example
Clock dimension: 3

Initial Zone Matrix:
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=0       <=0
     x |    <inf       <=0       <=0
     y |    <inf       <=0       <=0
```

### Example 2: Detailed Output (Constraints + Matrix)

```bash
./tck-matrix -d model.tck
```

**Output:**
```
=== TChecker Zone Matrix Visualization ===
System: timer_example
Clock dimension: 3

Initial State (Detailed):
Constraints: (0<=x && 0<=y && x==y)
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=0       <=0
     x |    <inf       <=0       <=0
     y |    <inf       <=0       <=0
```

### Example 3: Simulation Mode (Zone Evolution)

```bash
./tck-matrix -s 5 model.tck
```

**Output:**
```
=== TChecker Zone Matrix Visualization ===
System: timer_example
Clock dimension: 3

Initial Zone Matrix:
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=0       <=0
     x |    <inf       <=0       <=0
     y |    <inf       <=0       <=0

=== Simulation (5 steps) ===
Step 1:
Transition: <event>
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=0       <=0
     x |      <=8       <=0       <=0
     y |      <=8       <=0       <=0

Step 2:
Transition: <event>
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=-2       <=-2
     x |      <=15       <=0       <=0
     y |      <=15       <=0       <=0
```

### Example 4: Output to File

```bash
./tck-matrix -s 3 -o matrix_output.txt model.tck
```

The output will be saved to `matrix_output.txt` with the same format as stdout.

## Model Examples

### Simple Example Model

Create a basic timer model file:

```tck
system:timer_example

event:start
event:tick
event:timeout

int:1:0:2:0:state

process:Timer
clock:1:x
clock:1:y
location:Timer:idle{initial:}
location:Timer:running{invariant:x<=10&&y<=8}
location:Timer:critical{invariant:x<=15}
location:Timer:done{}

edge:Timer:idle:running:start{provided:state==0 : do:x=0;y=0;state=1}
edge:Timer:running:critical:tick{provided:y>=2&&x-y<=3 : do:state=2}
edge:Timer:running:done:timeout{provided:x>=8}
edge:Timer:critical:done:timeout{provided:x>=5}
edge:Timer:critical:critical:tick{provided:x<=12}
```

### Complex Example: Infinite Zone Model

```tck
system:infinite_zone_example

event:start_action
event:loop_action
event:end_action

process:System
clock:1:x
clock:1:y
location:System:start{initial:}
location:System:loop{invariant:x<=10}
location:System:end{}

edge:System:start:loop:start_action{do:x=0; y=0}
edge:System:loop:loop:loop_action{provided:x==10 : do:x=0}
edge:System:loop:end:end_action{provided:y>=20 : do:x=0; y=0}
```

**Analysis of the Infinite Zone Model:**

```bash
# Display initial state
./tck-matrix -i infinite_zone_example.tck
```

**Expected Output:**
```
=== TChecker Zone Matrix Visualization ===
System: infinite_zone_example
Clock dimension: 3

Initial Zone Matrix:
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=0       <=0
     x |    <inf       <=0       <=0
     y |    <inf       <=0       <=0
```

```bash
# Simulate evolution showing infinite zone growth
./tck-matrix -s 10 infinite_zone_example.tck
```

This model demonstrates how zones can grow infinitely due to the loop structure where `x` is reset while `y` continues to grow.

## Advanced Usage Scenarios

### Research and Development

```bash
# Analyze zone evolution for algorithm research
./tck-matrix -s 50 -o research_analysis.txt complex_model.tck
```

### Model Debugging

```bash
# Debug complex timing constraints
./tck-matrix -d problematic_model.tck | grep -E "(<=|<|>=|>)"
```

### Educational Use

```bash
# Create teaching materials showing DBM evolution
./tck-matrix -d -s 3 -o teaching_example.txt simple_model.tck
```

### Batch Processing

```bash
#!/bin/bash
# Analyze multiple models
for model in *.tck; do
    echo "Analyzing: $model"
    ./tck-matrix -i "$model" > "${model%.tck}_initial.txt"
    ./tck-matrix -s 5 "$model" > "${model%.tck}_simulation.txt"
done
```

## Understanding DBM Matrix Format

### Matrix Interpretation

- **Rows and Columns**: Clock names (0 represents reference clock)
- **Matrix Entry [i,j]**: Represents constraint `xi - xj ≤ c`
- **Special Values**:
  - `<=N`: Less than or equal to N
  - `<N`: Strictly less than N
  - `<inf`: Infinity (no constraint)

### Example Matrix Reading

```
       |       0        x        y
-------+--------------------------
     0 |      <=0       <=-2      <=-2
     x |      <=15       <=0       <=0
     y |      <=15       <=0       <=0
```

This represents:
- `0 - x ≤ -2` → `x ≥ 2`
- `0 - y ≤ -2` → `y ≥ 2`
- `x - 0 ≤ 15` → `x ≤ 15`
- `y - 0 ≤ 15` → `y ≤ 15`
- `x - y ≤ 0` and `y - x ≤ 0` → `x = y`

## Integration with Other TChecker Tools

### Combined Analysis

```bash
# Combine reachability analysis with zone visualization
./tck-reach -a reach model.tck
./tck-matrix -s 10 model.tck

# Compare different algorithms
./tck-reach -a covreach model.tck
./tck-matrix -d model.tck
```

### Verification Workflow

```bash
# Complete verification workflow
echo "1. Syntax check..."
./tck-syntax model.tck

echo "2. Initial zone analysis..."
./tck-matrix -i model.tck

echo "3. Reachability analysis..."
./tck-reach -a reach model.tck

echo "4. Zone evolution simulation..."
./tck-matrix -s 20 model.tck
```

## Performance Considerations

- **Large Models**: Use `-i` option for quick initial analysis
- **Long Simulations**: Output to file (`-o`) for better performance
- **Memory Usage**: Zone graphs can grow exponentially; monitor system resources

## Troubleshooting

### Common Issues

1. **File Not Found**: Ensure model file path is correct
2. **Parse Errors**: Check model syntax with `tck-syntax`
3. **Empty Output**: Model may have no reachable states
4. **Large Output**: Use file output (`-o`) for better handling

### Debug Commands

```bash
# Check if model is syntactically correct
./tck-syntax model.tck

# Verify model has initial states
./tck-matrix -i model.tck

# Check for reachable states
./tck-reach -a reach model.tck
```

This tool provides comprehensive DBM visualization capabilities essential for research, education, and debugging in timed automata verification.