# Interprocess Communication Using Shared Memory and Semaphores

This project implements a bidirectional interprocess communication (IPC) system in C using shared memory and semaphores on UNIX-like systems. It features a synchronized producer-consumer model using circular buffers for efficient data exchange.

## Features

- Two-way communication using shared memory segments
- Semaphore-based synchronization to prevent race conditions
- Non-blocking I/O handling in the producer
- Robust error handling and cleanup of IPC resources

## File Descriptions

- `producer.c`: The producer program reads user input and writes it into shared memory.
- `consumer.c`: The consumer program reads data from shared memory, processes it (e.g., adds 1), and writes the result back.
- `shared_def.h`: Shared definitions including data structures, semaphore indices, constants, and utility functions like timestamp generation.

## How to Compile and Run

1. Compile both source files using `gcc`:
   ```bash
   gcc -o producer producer.c
   gcc -o consumer consumer.c
  ```bash
   ./producer
   ./consumer
