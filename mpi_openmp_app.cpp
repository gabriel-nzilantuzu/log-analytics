#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to analyze logs
void analyze_logs(const char **logs, int num_logs) {
    #pragma omp parallel for
    for (int i = 0; i < num_logs; i++) {
        printf("Thread %d analyzing log %d: %s\n", omp_get_thread_num(), i, logs[i]);
        // Simulate heavy computation with a sleep
        for (volatile int j = 0; j < 100000000; j++); // Simulating workload
    }
}

// Function to categorize logs
void categorize_logs(const char **logs, int num_logs) {
    #pragma omp parallel for
    for (int i = 0; i < num_logs; i++) {
        if (strstr(logs[i], "Error")) {
            printf("Thread %d: Log %d categorized as ERROR\n", omp_get_thread_num(), i);
        } else if (strstr(logs[i], "Warning")) {
            printf("Thread %d: Log %d categorized as WARNING\n", omp_get_thread_num(), i);
        } else if (strstr(logs[i], "Critical")) {
            printf("Thread %d: Log %d categorized as CRITICAL\n", omp_get_thread_num(), i);
        } else {
            printf("Thread %d: Log %d categorized as INFO\n", omp_get_thread_num(), i);
        }
    }
}

// Function to count the occurrences of a keyword in logs
void count_keyword_occurrences(const char **logs, int num_logs, const char *keyword) {
    int count = 0;

    #pragma omp parallel for reduction(+:count)
    for (int i = 0; i < num_logs; i++) {
        if (strstr(logs[i], keyword)) {
            count++;
        }
    }

    printf("Keyword '%s' found %d times.\n", keyword, count);
}

// Function to calculate a checksum for each log
void calculate_checksum(const char **logs, int num_logs) {
    #pragma omp parallel for
    for (int i = 0; i < num_logs; i++) {
        unsigned long checksum = 0;
        for (int j = 0; logs[i][j] != '\0'; j++) {
            checksum += logs[i][j];
        }
        printf("Thread %d: Log %d checksum: %lu\n", omp_get_thread_num(), i, checksum);
    }
}

int main() {
    int num_logs = 5;
    const char *logs[] = {
        "Error at module X",
        "Warning in service Y",
        "Timeout in Z",
        "Success message",
        "Critical issue in A"
    };

    omp_set_num_threads(4); // Use 4 threads

    printf("\n=== Step 1: Analyze Logs ===\n");
    analyze_logs(logs, num_logs);

    printf("\n=== Step 2: Categorize Logs ===\n");
    categorize_logs(logs, num_logs);

    printf("\n=== Step 3: Count Keyword Occurrences ===\n");
    count_keyword_occurrences(logs, num_logs, "Critical");

    printf("\n=== Step 4: Calculate Checksums ===\n");
    calculate_checksum(logs, num_logs);

    return 0;
}
