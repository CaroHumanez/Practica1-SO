// robot1.c - Empaca pares AB

#include <stdio.h>      
#include <stdlib.h>    
#include <fcntl.h>      
#include <sys/mman.h>  
#include <unistd.h>    
#include <string.h>
#include <semaphore.h>

int main() {
    int shm_fd;              
    char *cinta;
    sem_t *sem_prod, *sem_cons;
    int cp = 0;

    // 1. Abrir memoria compartida
    shm_fd = shm_open("/cinta_shm", O_RDWR, 0660);
    if (shm_fd == -1) {
        perror("No se pudo abrir la memoria compartida");
        exit(1);
    }

    // 2. Mapear memoria
    cinta = mmap(NULL, 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (cinta == MAP_FAILED) {
        perror("No se pudo mapear la memoria");
        close(shm_fd);
        exit(1);
    }

    // 3. Abrir semáforos
    sem_prod = sem_open("/sem_prod", 0);
    if (sem_prod == SEM_FAILED) {
        perror("robot1: Error al abrir semáforo /sem_prod");
        exit(1);
    }

    sem_cons = sem_open("/sem_cons", 0);
    if (sem_cons == SEM_FAILED) {
        perror("robot1: Error al abrir semáforo /sem_cons");
        exit(1);
    }

    // 4. Ciclo principal
    while (1) {
        sem_wait(sem_cons); // Espera producto

        char producto[3];
        producto[0] = cinta[0];
        producto[1] = cinta[1];
        producto[2] = '\0';

        if (strcmp(producto, "AB") == 0) {
            printf("Robot 1 empaqueta productos AB\n");
            cp++;
            cinta[0] = '-';
            cinta[1] = '-';
            sem_post(sem_prod);
        } else if (strcmp(producto, "ZZ") == 0) {
            printf("Robot 1 recibió el ZZ de fin\n");
            sem_post(sem_prod); // liberar antes de salir
            break;
        } else {
            // No es su producto → liberar cinta
            printf("Robot 1 ignora producto %s y libera cinta\n", producto);
            sem_post(sem_prod);
        }
    }

    // 5. Enviar resultado por FIFO
    int fifo_fd = open("robot1_fifo", O_WRONLY);
    if (fifo_fd == -1) {
        perror("Error al abrir el FIFO robot1_fifo");
        return 1;
    }

    if (write(fifo_fd, &cp, sizeof(int)) == -1) {
        perror("Error al escribir en el FIFO robot1_fifo");
        close(fifo_fd);
        return 1;
    }

    // 6. Liberar recursos
    munmap(cinta, 2);
    close(shm_fd);
    close(fifo_fd);
    sem_close(sem_prod);
    sem_close(sem_cons);

    return 0;
}


