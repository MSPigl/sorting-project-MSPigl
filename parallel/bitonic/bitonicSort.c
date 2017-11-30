/*
 * MPI implementation of the bitonic sort algorithm
 * (Based on the implementation by Github user adrianlee)
 *
 * Matt Pigliavento
 * Siena College, Fall 2017
 */

#include <stdio.h>      
#include <stdlib.h>
#include <math.h>
#include <mpi.h>    

#define MASTER 0        // Who should do the final processing?
#define OUTPUT_NUM 10   // Number of elements to display in output

int myRank, numProcs;
int *array;
int array_size;

///////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////
int main(int argc, char * argv[]) {
    int i, j;

    // Initialization, get # of processes & this PID/rank
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

    // Initialize Array for Storing Random Numbers
    array_size = atoi(argv[1]) / numProcs;
    array = makeArray(array_size);

    // Cube Dimension
    int dimensions = log2(numProcs);

    // Start Timer before starting first sort operation (first iteration)
    if (process_rank == MASTER) {
        printf("Number of Processes spawned: %d\n", num_processes);
        timer_start = MPI_Wtime();
    }

    // Sequential Sort
    qsort(array, array_size, sizeof(int), ComparisonFunc);

    // Bitonic Sort follows
    for (i = 0; i < dimensions; i++) {
        for (j = i; j >= 0; j--) {
            // (window_id is even AND jth bit of process is 0)
            // OR (window_id is odd AND jth bit of process is 1)
            if (((process_rank >> (i + 1)) % 2 == 0 && (process_rank >> j) % 2 == 0) || ((process_rank >> (i + 1)) % 2 != 0 && (process_rank >> j) % 2 != 0)) {
                CompareLow(j);
            } else {
                CompareHigh(j);
            }
        }
    }

    // Blocks until all processes have finished sorting
    MPI_Barrier(MPI_COMM_WORLD);

    if (process_rank == MASTER) {
        timer_end = MPI_Wtime();

        printf("Displaying sorted array (only 10 elements for quick verification)\n");

        // Print Sorting Results
        for (i = 0; i < array_size; i++) {
            if ((i % (array_size / OUTPUT_NUM)) == 0) {
                printf("%d ",array[i]);
            }
        }
        printf("\n\n");

        printf("Time Elapsed (Sec): %f\n", timer_end - timer_start);
    }

    // Reset the state of the heap from Malloc
    free(array);

    // Done
    MPI_Finalize();
    return 0;
}

int log2(int n)
{
  return log(n) / log(2);  
}

///////////////////////////////////////////////////
// Comparison Function
///////////////////////////////////////////////////
int ComparisonFunc(const void * a, const void * b) {
    return ( * (int *)a - * (int *)b );
}

///////////////////////////////////////////////////
// Compare Low
///////////////////////////////////////////////////
void CompareLow(int j) {
    int i, min;

    /* Sends the biggest of the list and receive the smallest of the list */

    // Send entire array to paired H Process
    // Exchange with a neighbor whose (d-bit binary) processor number differs only at the jth bit.
    int send_counter = 0;
    int * buffer_send = malloc((array_size + 1) * sizeof(int));
    MPI_Send(
        &array[array_size - 1],     // entire array
        1,                          // one data item
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD              // default comm.
    );

    // Receive new min of sorted numbers
    int recv_counter;
    int * buffer_recieve = malloc((array_size + 1) * sizeof(int));
    MPI_Recv(
        &min,                       // buffer the message
        1,                          // one data item
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD,             // default comm.
        MPI_STATUS_IGNORE           // ignore info about message received
    );

    // Buffers all values which are greater than min send from H Process.
    for (i = 0; i < array_size; i++) {
        if (array[i] > min) {
            buffer_send[send_counter + 1] = array[i];
            send_counter++;
        } else {
            break;      // Important! Saves lots of cycles!
        }
    }

    buffer_send[0] = send_counter;

    // send partition to paired H process
    MPI_Send(
        buffer_send,                // Send values that are greater than min
        send_counter,               // # of items sent
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD              // default comm.
    );

    // receive info from paired H process
    MPI_Recv(
        buffer_recieve,             // buffer the message
        array_size,                 // whole array
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD,             // default comm.
        MPI_STATUS_IGNORE           // ignore info about message received
    );

    // Take received buffer of values from H Process which are smaller than current max
    for (i = 1; i < buffer_recieve[0] + 1; i++) {
        if (array[array_size - 1] < buffer_recieve[i]) {
            // Store value from message
            array[array_size - 1] = buffer_recieve[i];
        } else {
            break;      // Important! Saves lots of cycles!
        }
    }

    // Sequential Sort
    qsort(array, array_size, sizeof(int), ComparisonFunc);

    // Reset the state of the heap from Malloc
    free(buffer_send);
    free(buffer_recieve);

    return;
}


///////////////////////////////////////////////////
// Compare High
///////////////////////////////////////////////////
void CompareHigh(int j) {
    int i, max;

    // Receive max from L Process's entire array
    int recv_counter;
    int * buffer_recieve = malloc((array_size + 1) * sizeof(int));
    MPI_Recv(
        &max,                       // buffer max value
        1,                          // one item
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD,             // default comm.
        MPI_STATUS_IGNORE           // ignore info about message received
    );

    // Send min to L Process of current process's array
    int send_counter = 0;
    int * buffer_send = malloc((array_size + 1) * sizeof(int));
    MPI_Send(
        &array[0],                  // send min
        1,                          // one item
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD              // default comm.
    );

    // Buffer a list of values which are smaller than max value
    for (i = 0; i < array_size; i++) {
        if (array[i] < max) {
            buffer_send[send_counter + 1] = array[i];
            send_counter++;
        } else {
            break;      // Important! Saves lots of cycles!
        }
    }

    // Receive blocks greater than min from paired slave
    MPI_Recv(
        buffer_recieve,             // buffer message
        array_size,                 // whole array
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD,             // default comm.
        MPI_STATUS_IGNORE           // ignore info about message receiveds
    );
    recv_counter = buffer_recieve[0];

    // send partition to paired slave
    buffer_send[0] = send_counter;
    MPI_Send(
        buffer_send,                // all items smaller than max value
        send_counter,               // # of values smaller than max
        MPI_INT,                    // INT
        process_rank ^ (1 << j),    // paired process calc by XOR with 1 shifted left j positions
        0,                          // tag 0
        MPI_COMM_WORLD              // default comm.
    );

    // Take received buffer of values from L Process which are greater than current min
    for (i = 1; i < recv_counter + 1; i++) {
        if (buffer_recieve[i] > array[0]) {
            // Store value from message
            array[0] = buffer_recieve[i];
        } else {
            break;      // Important! Saves lots of cycles!
        }
    }

    // Sequential Sort
    qsort(array, array_size, sizeof(int), ComparisonFunc);

    // Reset the state of the heap from Malloc
    free(buffer_send);
    free(buffer_recieve);

    return;
}
