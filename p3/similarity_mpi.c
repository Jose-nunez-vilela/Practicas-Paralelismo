#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define DEBUG 0


#define M  1000000 // Number of sequences
#define N  200  // Number of bases per sequence

unsigned int g_seed = 0;

int fast_rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16) % 5;
}
// The distance between two bases
int base_distance(int base1, int base2){

  if((base1 == 4) || (base2 == 4)){
    return 3;
  }

  if(base1 == base2) {
    return 0;
  }

  if((base1 == 0) && (base2 == 3)) {
    return 1;
  }

  if((base2 == 0) && (base1 == 3)) {
    return 1;
  }

  if((base1 == 1) && (base2 == 2)) {
    return 1;
  }

  if((base2 == 1) && (base1 == 2)) {
    return 1;
  }

  return 2;
}

int main(int argc, char *argv[]) {
  int rank, numprocs;
  int i, j;

  int *data1 = NULL, *data2 = NULL;
  int *result = NULL;
  int *local_data1 = NULL, *local_data2 = NULL;
  int *local_result = NULL;

  int *sendcounts_data = NULL, *displs_data = NULL;
  int *recvcounts_result = NULL, *displs_result = NULL;
  double local_comm_time = 0.0, local_comp_time = 0.0;
  double local_total_time, program_total_time;
  double *all_comm_times = NULL, *all_comp_times = NULL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

  /* Reparto de filas válido incluso si M no es múltiplo del número de procesos. */
  int base_rows = M / numprocs;
  int extra_rows = M % numprocs;
  int local_rows = base_rows + (rank < extra_rows ? 1 : 0);

  local_data1 = (int *) malloc((local_rows * N > 0 ? local_rows * N : 1) * sizeof(int));
  local_data2 = (int *) malloc((local_rows * N > 0 ? local_rows * N : 1) * sizeof(int));
  local_result = (int *) malloc((local_rows > 0 ? local_rows : 1) * sizeof(int));

  if (local_data1 == NULL || local_data2 == NULL || local_result == NULL) {
    fprintf(stderr, "Process %d: memory allocation error\n", rank);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (rank == 0) {
    data1 = (int *) malloc(M * N * sizeof(int));
    data2 = (int *) malloc(M * N * sizeof(int));
    result = (int *) malloc(M * sizeof(int));

    sendcounts_data = (int *) malloc(numprocs * sizeof(int));
    displs_data = (int *) malloc(numprocs * sizeof(int));
    recvcounts_result = (int *) malloc(numprocs * sizeof(int));
    displs_result = (int *) malloc(numprocs * sizeof(int));
    all_comm_times = (double *) malloc(numprocs * sizeof(double));
    all_comp_times = (double *) malloc(numprocs * sizeof(double));

    if (data1 == NULL || data2 == NULL || result == NULL ||
        sendcounts_data == NULL || displs_data == NULL ||
        recvcounts_result == NULL || displs_result == NULL ||
        all_comm_times == NULL || all_comp_times == NULL) {
      fprintf(stderr, "Process 0: memory allocation error\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* Initialize Matrices */
    for(i = 0; i < M; i++) {
      for(j = 0; j < N; j++) {
        /* random with 20% gap proportion */
        data1[i * N + j] = fast_rand();
        data2[i * N + j] = fast_rand();
      }
    }

    int offset_rows = 0;
    for (i = 0; i < numprocs; i++) {
      int rows_i = base_rows + (i < extra_rows ? 1 : 0);
      sendcounts_data[i] = rows_i * N;
      displs_data[i] = offset_rows * N;
      recvcounts_result[i] = rows_i;
      displs_result[i] = offset_rows;
      offset_rows += rows_i;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  double t1 = MPI_Wtime();
  MPI_Scatterv(data1, sendcounts_data, displs_data, MPI_INT,
               local_data1, local_rows * N, MPI_INT,
               0, MPI_COMM_WORLD);

  MPI_Scatterv(data2, sendcounts_data, displs_data, MPI_INT,
               local_data2, local_rows * N, MPI_INT,
               0, MPI_COMM_WORLD);
  double t2 = MPI_Wtime();
  local_comm_time += t2 - t1;

  t1 = MPI_Wtime();
  for(i = 0; i < local_rows; i++) {
    local_result[i] = 0;
    for(j = 0; j < N; j++) {
      local_result[i] += base_distance(local_data1[i * N + j], local_data2[i * N + j]);
    }
  }
  t2 = MPI_Wtime();
  local_comp_time += t2 - t1;

  t1 = MPI_Wtime();
  MPI_Gatherv(local_result, local_rows, MPI_INT,
              result, recvcounts_result, displs_result, MPI_INT,
              0, MPI_COMM_WORLD);
  t2 = MPI_Wtime();
  local_comm_time += t2 - t1;

  /* Se recogen los tiempos individuales para que solo imprima el proceso 0. */
  MPI_Gather(&local_comm_time, 1, MPI_DOUBLE,
             all_comm_times, 1, MPI_DOUBLE,
             0, MPI_COMM_WORLD);

  MPI_Gather(&local_comp_time, 1, MPI_DOUBLE,
             all_comp_times, 1, MPI_DOUBLE,
             0, MPI_COMM_WORLD);

  local_total_time = local_comm_time + local_comp_time;
  MPI_Reduce(&local_total_time, &program_total_time, 1, MPI_DOUBLE,
             MPI_MAX, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    if (DEBUG == 1) {
      int checksum = 0;
      for(i = 0; i < M; i++) {
        checksum += result[i];
      }
      printf("Checksum: %d\n", checksum);
    } else if (DEBUG == 2) {
      for(i = 0; i < M; i++) {
        printf(" %d \t ", result[i]);
      }
      printf("\n");
    }
    int checksum = 0;
    for(i = 0; i < M; i++) {
      checksum += result[i];
    }
    for (i = 0; i < numprocs; i++) {
      printf("Proceso %d. Tcomm = %lf, Tcomp = %lf, Ttotal = %lf\n",
        i, all_comm_times[i], all_comp_times[i],
        all_comm_times[i] + all_comp_times[i]);
      }
      
    printf("Checksum: %d\n", checksum);
    printf("Tiempo total del programa = %lf\n", program_total_time);
  }

  free(local_data1);
  free(local_data2);
  free(local_result);

  if (rank == 0) {
    free(data1);
    free(data2);
    free(result);
    free(sendcounts_data);
    free(displs_data);
    free(recvcounts_result);
    free(displs_result);
    free(all_comm_times);
    free(all_comp_times);
  }

  MPI_Finalize();
  return 0;
}
