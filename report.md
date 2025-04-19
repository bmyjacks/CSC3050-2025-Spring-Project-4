# CSC3050 Project 4: Cache Simulator Report

Group Members:                    
Student ID:          
Contribution:

Note: Only leave sections blank if they are not yet implemented. We will verify that your code matches the data in your
report. Any inconsistencies will result in double point deductions.

## 1. Part 1

### 1.1 Split Cache Design

Description of how the split cache (ICache and DCache) is implemented and the main differences compared to unified
cache.

### 1.2 Implementation Details

Detailed explanation of key implementation methods, especially the design of the ICache class and the simulation method
for parallel access.

### 1.3 Result Analysis

| Trace File | Cache Type    | Miss Count | Expected Miss Count | Total Cycles | Expected Total Cycles |
|------------|---------------|------------|---------------------|--------------|-----------------------|
| I.trace    | Split Cache   | 512        | 512                 | 41088        | 41088                 |
| I.trace    | Unified Cache | 384        | 384                 | 54912        | 54912                 |
| D.trace    | Split Cache   | 634        | 634                 | 67584        | 67584                 |
| D.trace    | Unified Cache | 507        | 507                 | 68141        | 68141                 |

Analysis of performance differences between split cache and unified cache for different trace files, explaining the
observed results.

## 2. Part 2

### Correct results after bug fixed:

| Cache Level | Read Count | Write Count | Hit Count | Miss Count | Miss Rate | Total Cycles |
|-------------|------------|-------------|-----------|------------|-----------|--------------|
| L1          | 181,708    | 50,903      | 177,911   | 54,700     | 23.5%     | 717,519      |
| L2          | 47,667     | 7,033       | 25,574    | 29,126     | 53.1%     | 888,292      |
| L3          | 26,984     | 2,142       | 21,596    | 7,530      | 25.9%     | 1,184,920    |

### My Results after bug fixed:

| Cache Level | Read Count | Write Count | Hit Count | Miss Count | Miss Rate | Total Cycles |
|-------------|------------|-------------|-----------|------------|-----------|--------------|
| L1          |            |             |           |            |           |              |
| L2          |            |             |           |            |           |              |
| L3          |            |             |           |            |           |              |

## 3. Part 3

### Optimization Techniques Application and Results

(Choose one optimization technique for each trace file)

*No Optimization* means, just use the initial code to run simulation.

| Trace       | Optimization    | Miss Count | Miss Rate | Expected Miss Rate |
|-------------|-----------------|------------|-----------|--------------------|
| test1.trace | No Optimization |            |           | None               |
| test1.trace |                 |            |           | 0.371%             |
| test2.trace | No Optimization |            |           | None               |
| test2.trace |                 |            |           | 5.803%             |
| test3.trace | No Optimization |            |           | None               |
| test3.trace |                 |            |           | 0.372%             |

## 4. Part 4

### 4.1 Performance Comparison (L1 Level)

| Implementation | Read Count | Write Count | Hit Count | Miss Count | Miss Rate | Total Cycles |
|----------------|------------|-------------|-----------|------------|-----------|--------------|
| matmul0        |            |             |           |            |           |              |
| matmul1        |            |             |           |            |           |              |
| matmul2        |            |             |           |            |           |              |
| matmul3        |            |             |           |            |           |              |
| matmul4        |            |             |           |            |           |              |

### 4.2 Analysis of Performance Differences between matmul1, 2, 3
