# Parallel Air Quality Data Processing System

## Project Overview

This repository contains the source code and documentation for a high-performance, parallel air quality data processing system. This system is designed to parse and analyze air quality data efficiently using a combination of OpenMP for multithreading and MPI (Message Passing Interface) with POSIX shared memory. The primary focus is on calculating the Average Air Quality Index (AQI) across various geographic locations and timeframes.

## Features

- **High-Performance Data Parsing**: Utilizes OpenMP for parallel processing of CSV files to optimize CPU usage across multiple cores.
- **Efficient Data Transfer**: Employs POSIX shared memory for fast data transfer between parsing and analysis stages, reducing latency compared to traditional file-based I/O.
- **Scalable Data Analysis**: Leverages MPI for distributing tasks across multiple nodes, which enhances scalability and performance in distributed computing environments.
- **Real-Time Data Visualization**: Integrates Python plotting scripts via mpi4py, facilitating real-time visualization of AQI trends in a mixed C++ and Python environment.

## Getting Started

### Prerequisites

Ensure you have the following installed:
- GCC with OpenMP support
- MPI compiler (e.g., MPICH, OpenMPI)
- Python 3 with mpi4py
- nlohmann/json for JSON handling in C++

### Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/cxx5208/Parallel-Air-Quality-Data-Processing-System.git
   ```
2. Navigate to the project directory:
   ```bash
   cd Parallel-Air-Quality-Data-Processing-System
   ```

### Usage

There are two main components of the system:

1. **Shared Memory Implementation**
   ```bash
   source venv/bin/activate
   g++ -fopenmp -o shm_parser shm_parser.cpp
   ./shm_parser
   mpicxx -I/opt/homebrew/include -o shm_analysis shm_analysis.cpp
   mpiexec -np 1 ./shm_analysis : -np 1 python shm_plot.py
   ```

2. **File System Implementation**
   ```bash
   g++ -fopenmp -o file_parser file_parser.cpp
   ./file_parser
   g++ -fopenmp -I/opt/homebrew/include -o file_analysis file_analysis.cpp
   ./file_analysis
   python3 file_plot.py
   ```

## Architecture
![image](https://github.com/cxx5208/CMPE-275-02/assets/76988460/fe50b223-7347-4265-b5ca-6125d0a4631d)

The system architecture is designed to maximize efficiency and scalability. It involves several stages of data handling, from initial parsing to final visualization:

- **Data Parsing**: Data is parsed using OpenMP, and stored in a POSIX shared memory segment for rapid access.
- **Data Analysis**: The parsed data is quickly accessed by the analysis component, which continues to use OpenMP and begins to incorporate MPI for distributing the computation.
- **Visualization**: Analysis results are passed to Python scripts for visualization using mpi4py.

## Performance

The system is benchmarked to provide insights into the performance improvements with varying configurations, highlighting the benefits of parallel processing and shared memory usage:

- **Parsing Performance**: Demonstrates the decrease in parsing time with increased thread count using OpenMP.
- **Shared Memory vs. File System**: Analyzes the trade-offs between using shared memory and traditional file system based approaches.


