#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int MPI_BinomialBcast(void *buffer, int count, MPI_Datatype datatype,
                      int root, MPI_Comm comm)
{
    int rank, numprocs, i;
    int step;

    int err = MPI_Comm_rank(comm, &rank);
    if (err != MPI_SUCCESS) return err;
    err = MPI_Comm_size(comm, &numprocs);
    if (err != MPI_SUCCESS) return err;

    for (i = 1; (step = (1 << (i - 1))) < numprocs; i++) {
        if (rank < step) {
            int dest = rank + step;
            if (dest < numprocs) {
                err = MPI_Send(buffer, count, datatype, dest, 0, comm);
                if (err != MPI_SUCCESS) return err;
            }
        } else if (rank < 2 * step) {
            int src = rank - step;
            err = MPI_Recv(buffer, count, datatype, src, 0, comm, MPI_STATUS_IGNORE);
            if (err != MPI_SUCCESS) return err;
        }
    }

    return MPI_SUCCESS;
}

int MPI_FlattreeColectiva(const void *sendbuf, void *recvbuf, int count,
                          MPI_Datatype datatype, MPI_Op op,
                          int root, MPI_Comm comm)
{
    int rank, numprocs, i;

    int err = MPI_Comm_rank(comm, &rank);
    if (err != MPI_SUCCESS) return err;
    err = MPI_Comm_size(comm, &numprocs);
    if (err != MPI_SUCCESS) return err;

    if (rank == root) {
        *((int *)recvbuf) = *((const int *)sendbuf);
        for (i = 1; i < numprocs; i++) {
            int parcial;
            err = MPI_Recv(&parcial, count, datatype, i, 0, comm, MPI_STATUS_IGNORE);
            if (err != MPI_SUCCESS) return err;
            *((int *)recvbuf) += parcial;
        }
    } else {
        err = MPI_Send(sendbuf, count, datatype, root, 0, comm);
        if (err != MPI_SUCCESS) return err;
    }

    return MPI_SUCCESS;
}

void inicializaCadena(char *cadena, int n)
{
    int i;
    for (i = 0; i < n / 2; i++)          cadena[i] = 'A';
    for (i = n / 2; i < 3 * n / 4; i++)  cadena[i] = 'C';
    for (i = 3 * n / 4; i < 9 * n / 10; i++) cadena[i] = 'G';
    for (i = 9 * n / 10; i < n; i++)     cadena[i] = 'T';
}

int main(int argc, char *argv[])
{
    int rank, numprocs;
    int n, i;
    int count_local = 0;
    int count_total = 0;
    char *cadena;
    char L;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

    if (rank == 0) {
        if (argc != 3) {
            printf("Numero incorrecto de parametros\n");
            printf("La sintaxis debe ser: program n L\n");
            printf("  program es el nombre del ejecutable\n");
            printf("  n es el tamanho de la cadena a generar\n");
            printf("  L es la letra (A, C, G o T)\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        n = atoi(argv[1]);
        L = argv[2][0];
    }

    MPI_BinomialBcast(&n, 1, MPI_INT,  0, MPI_COMM_WORLD);
    MPI_BinomialBcast(&L, 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    cadena = (char *) malloc(n * sizeof(char));
    inicializaCadena(cadena, n);

    for (i = rank; i < n; i += numprocs) {
        if (cadena[i] == L) {
            count_local++;
        }
    }

    MPI_FlattreeColectiva(&count_local, &count_total, 1,
                          MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("El numero de apariciones de la letra %c es %d\n", L, count_total);
    }

    free(cadena);
    MPI_Finalize();
    return 0;
}