#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <time.h>

// Function to analyze logs
void analyze_logs(const char **logs, int num_logs, int rank, int num_procs) {
    // Divide the logs among the processes
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    for (int i = start_idx; i < end_idx; i++) {
        printf("Rank %d (Thread %d) analyzing log %d: %s\n", rank, omp_get_thread_num(), i, logs[i]);
        // Simulate heavy computation with a sleep
        for (volatile int j = 0; j < 100000000; j++); // Simulating workload
    }
}

// Function to categorize logs
void categorize_logs(const char **logs, int num_logs, int rank, int num_procs) {
    // Divide the logs among the processes
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    for (int i = start_idx; i < end_idx; i++) {
        if (strstr(logs[i], "Error")) {
            printf("Rank %d (Thread %d): Log %d categorized as ERROR\n", rank, omp_get_thread_num(), i);
        } else if (strstr(logs[i], "Warning")) {
            printf("Rank %d (Thread %d): Log %d categorized as WARNING\n", rank, omp_get_thread_num(), i);
        } else if (strstr(logs[i], "Critical")) {
            printf("Rank %d (Thread %d): Log %d categorized as CRITICAL\n", rank, omp_get_thread_num(), i);
        } else {
            printf("Rank %d (Thread %d): Log %d categorized as INFO\n", rank, omp_get_thread_num(), i);
        }
    }
}

// Function to count the occurrences of a keyword in logs
void count_keyword_occurrences(const char **logs, int num_logs, const char *keyword, int rank, int num_procs) {
    int count = 0;

    // Divide the logs among the processes
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    for (int i = start_idx; i < end_idx; i++) {
        if (strstr(logs[i], keyword)) {
            count++;
        }
    }

    printf("Rank %d: Keyword '%s' found %d times.\n", rank, keyword, count);
}

// Function to calculate a checksum for each log
void calculate_checksum(const char **logs, int num_logs, int rank, int num_procs) {
    // Divide the logs among the processes
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    for (int i = start_idx; i < end_idx; i++) {
        unsigned long checksum = 0;
        for (int j = 0; logs[i][j] != '\0'; j++) {
            checksum += logs[i][j];
        }
        printf("Rank %d (Thread %d): Log %d checksum: %lu\n", rank, omp_get_thread_num(), i, checksum);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    int num_logs = 5;
    const char *logs[] = {
        "Error at module X",
        "Warning in service Y",
        "Timeout in Z",
        "Success message",
        "Critical issue in A"
    };

    double start_time, end_time;
    
    // Start timer for the master rank (rank 0)
    if (rank == 0) {
        start_time = MPI_Wtime();
    }

    omp_set_num_threads(4); // Use 4 threads per process

    // Execute log analysis, categorization, and other tasks
    printf("\n=== Step 1: Analyze Logs (Rank %d) ===\n", rank);
    analyze_logs(logs, num_logs, rank, num_procs);

    printf("\n=== Step 2: Categorize Logs (Rank %d) ===\n", rank);
    categorize_logs(logs, num_logs, rank, num_procs);

    printf("\n=== Step 3: Count Keyword Occurrences (Rank %d) ===\n", rank);
    count_keyword_occurrences(logs, num_logs, "Critical", rank, num_procs);

    printf("\n=== Step 4: Calculate Checksums (Rank %d) ===\n", rank);
    calculate_checksum(logs, num_logs, rank, num_procs);

    // End timer for the master rank (rank 0)
    if (rank == 0) {
        end_time = MPI_Wtime();
        printf("\n=== Total execution time: %f seconds ===\n", end_time - start_time);
    }

    MPI_Finalize();
    return 0;
}
