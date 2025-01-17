#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <time.h>
#include <libwebsockets.h>

// WebSocket data handling callback
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

// Function to send JSON data via WebSocket
void send_websocket_data(const char *json_data) {
    struct lws_context_creation_info info;
    struct lws_client_connect_info i;
    struct libwebsocket_context *context;
    struct libwebsocket *wsi;

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN; // We're a client, so no listening port
    info.protocols = (struct libwebsocket_protocols[]) {{
        "http-only", websocket_callback, 0, 0,
    }, {NULL, NULL, 0, 0}}; // Single protocol

    context = libwebsocket_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "Error creating WebSocket context.\n");
        return;
    }

    memset(&i, 0, sizeof(i));
    i.context = context;
    i.address = "log-analytics.ns.namespaxe.com";
    i.port = 80; // Use the appropriate port for WebSocket
    i.path = "/logger";
    i.host = i.address;
    i.origin = i.address;
    i.protocol = "http-only";
    i.local_protocol_name = NULL;

    // Connect to WebSocket
    wsi = libwebsocket_client_connect_via_info(&i);
    if (wsi == NULL) {
        fprintf(stderr, "WebSocket connection failed.\n");
        return;
    }

    // Wait for the connection to be established
    while (libwebsocket_service(context, 0) >= 0) {
        if (libwebsocket_get_peer_address(wsi) != NULL) {
            break;
        }
    }

    // Send JSON data
    libwebsocket_write(wsi, (unsigned char *)json_data, strlen(json_data), LWS_WRITE_TEXT);

    // Wait for the server to receive the message
    libwebsocket_service(context, 0);

    // Close WebSocket connection
    libwebsocket_context_destroy(context);
}

// Function to gather and send the final JSON data to WebSocket
void gather_and_send_to_websocket(char gathered_data[][8192], int num_procs) {
    double start_time, end_time;
    char final_json[65536] = "{\"all_tasks\": [";

    // Collect gathered data from each process
    for (int i = 0; i < num_procs; i++) {
        strcat(final_json, gathered_data[i]);
        strcat(final_json, ",");
    }
    final_json[strlen(final_json) - 1] = ']'; // Remove trailing comma
    sprintf(final_json + strlen(final_json), "}");

    // Send the compiled data to WebSocket server
    send_websocket_data(final_json);
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
        gather_and_send_to_websocket(gathered_data, num_procs);
    }

    MPI_Finalize();
    return 0;
}
