#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <time.h>
#include <curl/curl.h>
#include <libwebsockets.h>

static int websocket_callback(struct libwebsocket_context *context, struct libwebsocket *wsi,
                              enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("WebSocket connection established.\n");
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Received data: %s\n", (char *)in);
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("WebSocket connection closed.\n");
            break;
        default:
            break;
    }
    return 0;
}

void connect_socket(const char *url, const char *json_data) {
    struct lws_context_creation_info info;
    struct lws_client_connect_info i;
    struct libwebsocket_context *context;
    struct libwebsocket *wsi;

    // Initialize the WebSocket context
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN; // We're a client, so no listening port
    info.protocols = (struct libwebsocket_protocols[]) {{
        "http-only", websocket_callback, 0, 0,
    }, {NULL, NULL, 0, 0}}; // Single protocol

    // Create WebSocket context
    context = libwebsocket_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "Error creating WebSocket context.\n");
        return;
    }

    // Set up connection information
    memset(&i, 0, sizeof(i));
    i.context = context;
    i.address = "log-analytics.ns.namespaxe.com"; // WebSocket server address
    i.port = 80; // WebSocket port (change if needed)
    i.path = "/logger"; // WebSocket path
    i.host = i.address;
    i.origin = i.address;
    i.protocol = "http-only"; // Protocol to use

    // Connect to WebSocket server
    wsi = libwebsocket_client_connect_via_info(&i);
    if (wsi == NULL) {
        fprintf(stderr, "WebSocket connection failed.\n");
        libwebsocket_context_destroy(context);
        return;
    }

    // Wait for the WebSocket connection to be established
    while (libwebsocket_service(context, 0) >= 0) {
        if (libwebsocket_get_peer_address(wsi) != NULL) {
            break;
        }
    }

    // Send the JSON data through the WebSocket
    libwebsocket_write(wsi, (unsigned char *)json_data, strlen(json_data), LWS_WRITE_TEXT);

    // Wait for the server to receive the message
    libwebsocket_service(context, 0);

    // Close the WebSocket connection
    libwebsocket_context_destroy(context);
}

// Function to analyze logs
void analyze_logs(const char **logs, int num_logs, int rank, int num_procs, char *output) {
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    sprintf(output + strlen(output), "\"analyzed_logs\": [");
    for (int i = start_idx; i < end_idx; i++) {
        sprintf(output + strlen(output), "{\"log_id\": %d, \"log\": \"%s\", \"rank\": %d},", i, logs[i], rank);
    }
    output[strlen(output) - 1] = ']'; // Remove trailing comma
}

// Function to categorize logs
void categorize_logs(const char **logs, int num_logs, int rank, int num_procs, char *output) {
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

    sprintf(output + strlen(output), ", \"categories\": [");
    for (int i = start_idx; i < end_idx; i++) {
        const char *category = strstr(logs[i], "Error") ? "ERROR" :
                               strstr(logs[i], "Warning") ? "WARNING" :
                               strstr(logs[i], "Critical") ? "CRITICAL" : "INFO";
        sprintf(output + strlen(output), "{\"log_id\": %d, \"category\": \"%s\", \"rank\": %d},", i, category, rank);
    }
    output[strlen(output) - 1] = ']'; // Remove trailing comma
}

// Function to count occurrences of a keyword
void count_keyword_occurrences(const char **logs, int num_logs, const char *keyword, int rank, int num_procs, char *output) {
    int count = 0;
    int logs_per_process = num_logs / num_procs;
    int start_idx = rank * logs_per_process;
    int end_idx = (rank == num_procs - 1) ? num_logs : start_idx + logs_per_process;

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
    for (int i = start_idx; i < end_idx; i++) {
        unsigned long checksum = 0;
        for (int j = 0; logs[i][j] != '\0'; j++) {
            checksum += logs[i][j];
        }
        sprintf(output + strlen(output), "{\"log_id\": %d, \"checksum\": %lu, \"rank\": %d},", i, checksum, rank);
    }
    output[strlen(output) - 1] = ']'; // Remove trailing comma
}

// Function to send JSON data via webhook
void connect_socket(const char *url, const char *json_data) {
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

    int num_logs = 5;
    const char *logs[] = {
        "Error at module X",
        "Warning in service Y",
        "Timeout in Z",
        "Success message",
        "Critical issue in A"
    };

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

        // Send to webhook
        connect_socket("https://log-analytics.ns.namespaxe.com/logger", final_json);
    }

    MPI_Finalize();
    return 0;
}
