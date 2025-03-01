FROM ubuntu:20.04

# Set the environment to non-interactive to prevent prompts during installation
ENV DEBIAN_FRONTEND=noninteractive
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# Install required dependencies: OpenMPI, OpenMP, and build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    openmpi-bin \
    libcurl4-openssl-dev \
    libwebsockets-dev \
    libssl-dev \
    libcrypto++-dev \
    libomp-dev \
    mpich \
    tzdata \
    jq \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user
RUN useradd -ms /bin/bash mpiuser

# Set the user to 'mpiuser' for subsequent operations
USER mpiuser

# Set the working directory for the non-root user
WORKDIR /app

# Copy your MPI + OpenMP C++ code into the container
COPY --chown=mpiuser:mpiuser . /app

# Compile your application
RUN mpic++ -fopenmp -o mpi_openmp_app mpi_openmp_app.cpp -lcurl -lwebsockets -lssl -lcrypto

# Command to run the application with oversubscribe and handle graceful termination
CMD ["sh", "-c", "trap 'echo SIGTERM received; echo Application terminated gracefully; exit 0' SIGTERM; mpirun --oversubscribe -np 2 ./mpi_openmp_app && echo 'Application completed!'; sleep 5"]

