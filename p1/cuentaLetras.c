#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

void inicializaCadena(char *cadena, int n){
    int i;
    for(i = 0; i < n/2; i++){
        cadena[i] = 'A';
    }
    for(i = n/2; i < 3*n/4; i++){
        cadena[i] = 'C';
    }
    for(i = 3*n/4; i < 9*n/10; i++){
        cadena[i] = 'G';
    }
    for(i = 9*n/10; i < n; i++){
        cadena[i] = 'T';
    }
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

    if(rank == 0){
        if(argc != 3){
            printf("Numero incorrecto de parametros\n");
            printf("La sintaxis debe ser: program n L\n");
            printf("  program es el nombre del ejecutable\n");
            printf("  n es el tamaño de la cadena a generar\n");
            printf("  L es la letra de la que se quiere contar apariciones (A, C, G o T)\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        n = atoi(argv[1]);
        L = argv[2][0];

        for(i = 1; i < numprocs; i++){
            MPI_Send(&n, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&L, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&L, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }


    
    cadena = (char *) malloc(n * sizeof(char));
    inicializaCadena(cadena, n);

    for(i = rank; i < n; i += numprocs){
        if(cadena[i] == L){
            count_local++;
        }
    }

    if(rank == 0){
        count_total = count_local;

        for(i = 1; i < numprocs; i++){
            int parcial;
            MPI_Recv(&parcial, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            count_total += parcial;
        }

        printf("El numero de apariciones de la letra %c es %d\n", L, count_total);
    } else {
        MPI_Send(&count_local, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    free(cadena);
    MPI_Finalize();
    return 0;
}