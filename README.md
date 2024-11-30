
# Memory Simulation System

This project simulates a basic memory system using multi-threaded and multi-process programming techniques. 
The system models various components of a memory management system, including processes, an MMU, an evicter, a printer, and a hard disk.

## Features

### Components

1. **Processes**:
   - **Process 1** and **Process 2**:
     - Simulate a CPU process requesting memory access.
     - Memory access is either a read or a write, determined probabilistically.

2. **Memory Management Unit (MMU)**:
   - Handles memory requests.
   - Simulates hit/miss logic and writes to memory.
   - Communicates with the hard disk and the evicter.

3. **Evicter**:
   - Frees up memory when it's full, using a FIFO scheme with a clock algorithm.
   - Writes dirty pages to the hard disk if necessary.

4. **Printer**:
   - Takes periodic snapshots of memory.
   - Outputs memory state: `0` for valid/clean, `1` for valid/dirty, and `-` for invalid pages.

5. **Hard Disk (HD)**:
   - Processes requests for page loads and writes from/to memory.

### Example Memory Snapshot (N=5)
```
0|-
1|0
2|0
3|1
4|-
```

### System Behavior

- The simulation runs for a predefined time (`SIM_TIME`).
- Memory state changes dynamically as processes make requests and the MMU processes them.
- Snapshots provide insight into memory usage over time.

### Parameters

Default values:
```c
#define N 5                        // Number of memory slots
#define WR_RATE 0.5                // Write request probability
#define HIT_RATE 0.5               // Memory hit probability
#define MEM_WR_T 1000              // Write time (nanoseconds)
#define HD_ACCS_T 1000000          // Hard disk access time (nanoseconds)
#define TIME_BETWEEN_SNAPSHOTS 100000000 // Snapshot interval (nanoseconds)
#define SIM_TIME 1                 // Simulation duration (seconds)
```

## How to Build and Run

1. Build the project:
   ```bash
   make
   ```

2. Run the simulation:
   ```bash
   ./memory_simulation
   ```

## Dependencies

- POSIX Threads (pthreads)
- IPC (Inter-Process Communication) facilities like `msgget`, `msgsnd`, `msgrcv`.

## File Structure

- `main.c`: Contains the implementation of the memory system simulation.
- `makefile`: Automates the build process.

## Notes

- The simulation ensures no deadlocks or race conditions.
- Robust error handling for all system calls and pthread operations.
