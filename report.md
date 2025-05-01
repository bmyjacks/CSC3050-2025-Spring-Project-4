# CSC3050 Project 4: Cache Simulator Report

Group Members: BAO, Mingyi                   
Student ID: 123090003

## 1. Part 1

### 1.1 Split Cache Design

In a split cache, the instruction cache (ICache) and data cache (DCache) are separate entities. This means that
instructions and data are stored in different caches, allowing for more efficient access patterns.

However, in a unified cache, both instructions and data are stored in the same cache. This can lead to contention
between instruction and data accesses, as they compete for the same cache space.

### 1.2 Implementation Details

Both `ICache` and `DCache` classes inherit from the `Cache` class. But the `ICache` class has a different
`writeBlockToLowerLevel` since it does not need to write back to the lower level cache. The size of the `ICache` and
`DCache` is half of the unified cache size.

For the simulation method, we use a loop to iterate through the trace file and access the cache. We then take the
maximum of the `ICache` and `DCache` cycles to simulate parallel access.

### 1.3 Result Analysis

| Trace File | Cache Type    | Miss Count | Expected Miss Count | Total Cycles | Expected Total Cycles |
|------------|---------------|------------|---------------------|--------------|-----------------------|
| I.trace    | Split Cache   | 512        | 512                 | 41088        | 41088                 |
| I.trace    | Unified Cache | 384        | 384                 | 54912        | 54912                 |
| D.trace    | Split Cache   | 634        | 634                 | 67584        | 67584                 |
| D.trace    | Unified Cache | 507        | 507                 | 68141        | 68141                 |

For the `I.trace` file, there are 3072 instructions accesses and 2560 data accesses. The split cache is faster than
unified cache as the instruction cache can be accessed in parallel with the data cache. But as the cache size is small,
miss count is higher.

For the `D.trace` file, there are 952 instructions accesses and 6780 data accesses. The split cache is little faster
than unified cache as it cannot benefit from parallel access as the workload are unbalanced.

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
| L1          | 181708     | 50903       | 177911    | 54700      | 23.52%    | 717519       |
| L2          | 47667      | 7033        | 25608     | 29092      | 53.18%    | 886984       |
| L3          | 26959      | 2133        | 21562     | 7530       | 25.88%    | 1184240      |

## 3. Part 3

### Optimization Techniques Application and Results

(Choose one optimization technique for each trace file)

*No Optimization* means, just use the initial code to run simulation.

| Trace       | Optimization    | Miss Count | Miss Rate | Expected Miss Rate |
|-------------|-----------------|------------|-----------|--------------------|
| test1.trace | No Optimization | 102656     | 99.26%    | None               |
| test1.trace | FIFO            | 384        | 0.37%     | 0.371%             |
| test2.trace | No Optimization | 100013     | 98.53%    | None               |
| test2.trace | Prefetch        | 1900       | 1.87%     | 5.803%             |
| test3.trace | No Optimization | 51712      | 50.12%    | None               |
| test3.trace | Victim Cache    | 384        | 0.37%     | 0.372%             |

## 4. Part 4

### 4.1 Performance Comparison (L1 Level)

| Implementation | Read Count | Write Count | Hit Count | Miss Count | Miss Rate | Total Cycles |
|----------------|------------|-------------|-----------|------------|-----------|--------------|
| matmul0        | 786432     | 262144      | 749118    | 299458     | 28.56%    | 3148878      |
| matmul1        | 528384     | 4096        | 228926    | 303554     | 57.01%    | 2690118      |
| matmul2        | 786432     | 262144      | 1011136   | 37440      | 3.57%     | 1572920      |
| matmul3        | 786432     | 262144      | 520192    | 528384     | 50.39%    | 6844160      |
| matmul4        | 540672     | 16384       | 548870    | 8186       | 1.47%     | 638782       |

### 4.2 Analysis of Performance Differences between matmul1, 2, 3

- matmul1: It uses `cij` to record an element in the result matrix then perform operations on it, causing fewer writes.
  Access `A` row-wise
  but `B` column-wise.
- matmul2: Both `B` and `C` accessed row-wise which is cache-friendly. And each `A[i, k]` is reused for the entire row
  of `C`.
- matmul3: Both `A` and `C` accessed column-wise causing low cache performance (No spatial locality).
