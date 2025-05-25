// robot2.c - Empaca pares AC

#include <stdio.h>      
#include <stdlib.h>    
#include <fcntl.h>      
#include <sys/mman.h>  
#include <unistd.h>    
#include <string.h>
#include <semaphore.h>
#include <errno.h>

#define SHM_NAME "/cinta_shm"
#define MUTEX "/mutex"
#define SEM_AC "/sem_AC"
#define DONE_AC "/done_AC"
#define FIFO_ROBOT2 "robot2_fifo"

int main() {
    int shm_fd;              
    char *cinta;
    sem_t *mutex, *sem_AC, *done_AC;
    int cp = 0;

    // 1. Abrir memoria compartida
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0660);
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

    // 3. Abrir semáforos correctos
    mutex = sem_open(MUTEX, 0);
    if (mutex == SEM_FAILED) {
        perror("robot2: Error al abrir semáforo mutex");
        exit(1);
    }

    sem_AC = sem_open(SEM_AC, 0);
    if (sem_AC == SEM_FAILED) {
        perror("robot2: Error al abrir semáforo sem_AC");
        exit(1);
    }

    done_AC = sem_open(DONE_AC, 0);
    if (done_AC == SEM_FAILED) {
        perror("robot2: Error al abrir semáforo done_AC");
        exit(1);
    }

    // 4. Ciclo principal
    while (1) {
        sem_wait(sem_AC);   // Esperar a que haya un par AC disponible

        sem_wait(mutex);    // Entrar sección crítica para leer la cinta

        // Leer producto en memoria compartida
        char producto[3];
        producto[0] = cinta[0];
        producto[1] = cinta[1];
        producto[2] = '\0';

        if (strcmp(producto, "AC") == 0) {
            printf("Robot 2 empaqueta productos AC\n");
            cp++;
            sem_post(done_AC);
            // Limpiar la cinta
            cinta[0] = '-';
            cinta[1] = '-';
        } else if (strcmp(producto, "ZZ") == 0) {
            printf("Robot 2 recibió el ZZ de fin\n");
            sem_post(mutex);
            break;
        } else {
            // En teoría no debería entrar aquí, pero por si acaso
            printf("Robot 2 ignoró producto %s\n", producto);
        }

        sem_post(mutex);    // Salir sección crítica
    }

    // 5. Enviar resultado por FIFO
    int fifo_fd = open(FIFO_ROBOT2, O_WRONLY);
    if (fifo_fd == -1) {
        perror("Error al abrir el FIFO robot2_fifo");
        return 1;
    }

    if (write(fifo_fd, &cp, sizeof(int)) == -1) {
        perror("Error al escribir en el FIFO robot2_fifo");
        close(fifo_fd);
        return 1;
    }

    close(fifo_fd);

    // 6. Liberar recursos
    munmap(cinta, 2);
    close(shm_fd);
    sem_close(mutex);
    sem_close(sem_AC);
    sem_close(done_AC);

    return 0;
}
