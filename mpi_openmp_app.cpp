#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <time.h>
#include <curl/curl.h>

// Function to analyze logs
void analyze_logs(const char **logs, int num_logs, int rank, int num_procs, char *output) {
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    sprintf(output + strlen(output), "\"analyzed_logs\": [");
    #pragma omp parallel for
    for (int i = start_idx; i < end_idx; i++) {
        char buffer[256];
        sprintf(buffer, "{\"log_id\": %d, \"log\": \"%s\", \"rank\": %d},", i, logs[i], rank);
        #pragma omp critical
        strcat(output, buffer);
    }
    output[strlen(output) - 1] = ']'; // Remove trailing comma
}

// Function to categorize logs
void categorize_logs(const char **logs, int num_logs, int rank, int num_procs, char *output) {
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    sprintf(output + strlen(output), ", \"categories\": [");
    #pragma omp parallel for
    for (int i = start_idx; i < end_idx; i++) {
        char buffer[256];
        const char *category = strstr(logs[i], "Error") ? "ERROR" :
                               strstr(logs[i], "Warning") ? "WARNING" :
                               strstr(logs[i], "Critical") ? "CRITICAL" : "INFO";
        sprintf(buffer, "{\"log_id\": %d, \"category\": \"%s\", \"rank\": %d},", i, category, rank);
        #pragma omp critical
        strcat(output, buffer);
    }
    output[strlen(output) - 1] = ']'; // Remove trailing comma
}

// Function to count occurrences of a keyword
void count_keyword_occurrences(const char **logs, int num_logs, const char *keyword, int rank, int num_procs, char *output) {
    int count = 0;
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    #pragma omp parallel for reduction(+:count)
    for (int i = start_idx; i < end_idx; i++) {
        if (strstr(logs[i], keyword)) {
            count++;
        }
    }
    sprintf(output + strlen(output), ", \"keyword_count\": {\"keyword\": \"%s\", \"count\": %d, \"rank\": %d}", keyword, count, rank);
}

// Function to calculate checksum
void calculate_checksum(const char **logs, int num_logs, int rank, int num_procs, char *output) {
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    sprintf(output + strlen(output), ", \"checksums\": [");
    #pragma omp parallel for
    for (int i = start_idx; i < end_idx; i++) {
        unsigned long checksum = 0;
        for (int j = 0; logs[i][j] != '\0'; j++) {
            checksum += logs[i][j];
        }
        char buffer[256];
        sprintf(buffer, "{\"log_id\": %d, \"checksum\": %lu, \"rank\": %d},", i, checksum, rank);
        #pragma omp critical
        strcat(output, buffer);
    }
    output[strlen(output) - 1] = ']'; // Remove trailing comma
}

// Function to send JSON data via webhook
void send_webhook(const char *url, const char *json_data) {
    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, (struct curl_slist*)curl_slist_append(NULL, "Content-Type: application/json"));
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "CURL Error: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    const char *logs[] = {
        "Error at module X",
        "Warning in service Y",
        "Timeout in Z",
        "Success message",
        "Critical issue in A",
        "Authorized attempt"
    };
    int num_logs = sizeof(logs) / sizeof(logs[0]);

    double start_time, end_time;
    if (rank == 0) {
        start_time = MPI_Wtime();
    }

    omp_set_num_threads(4); // Use 4 threads per process

    // Allocate space for analysis data
    char analysis_data[8192] = "{";
    sprintf(analysis_data + strlen(analysis_data), "\"rank\": %d, \"tasks\": {", rank);

    // Perform tasks
    analyze_logs(logs, num_logs, rank, num_procs, analysis_data);
    categorize_logs(logs, num_logs, rank, num_procs, analysis_data);
    count_keyword_occurrences(logs, num_logs, "Critical", rank, num_procs, analysis_data);
    calculate_checksum(logs, num_logs, rank, num_procs, analysis_data);

    sprintf(analysis_data + strlen(analysis_data), "}}"); // Close JSON

    // Print results for each rank
    printf("Rank %d: %s\n", rank, analysis_data);

    // Gather data on rank 0
    char gathered_data[num_procs][8192];
    MPI_Gather(analysis_data, 8192, MPI_CHAR, gathered_data, 8192, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        end_time = MPI_Wtime();

        // Compile final JSON
        char final_json[65536] = "{\"all_tasks\": [";
        for (int i = 0; i < num_procs; i++) {
            strcat(final_json, gathered_data[i]);
            strcat(final_json, ",");
        }
        final_json[strlen(final_json) - 1] = ']'; // Remove trailing comma
        sprintf(final_json + strlen(final_json), ", \"total_time\": %f}", end_time - start_time);

        // Print final JSON
        printf("Final JSON: %s\n", final_json);

        // Send to webhook
        send_webhook("https://log-analytics.ns.namespaxe.com/webhook", final_json);
    }

    MPI_Finalize();
    return 0;
}
