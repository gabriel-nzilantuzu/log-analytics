# Use an appropriate base image
FROM ubuntu:20.04

# Install required dependencies: OpenMPI, OpenMP, and build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    openmpi-bin \
    libomp-dev \
    mpich \
    && rm -rf /var/lib/apt/lists/*

# Copy your MPI + OpenMP C++ code into the container
COPY . /app

# Set the working directory
WORKDIR /app

# Compile your application
RUN mpic++ -fopenmp -o mpi_openmp_app mpi_openmp_app.cpp

# Command to run the application
CMD ["mpirun", "-np", "4", "./mpi_openmp_app"]