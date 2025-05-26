// robot3.c - Proceso que empaqueta pares BC desde la memoria compartida

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
#define SEM_BC "/sem_BC"
#define DONE_BC "/done_BC"
#define FIFO_ROBOT3 "robot3_fifo"

int main() {
    int shm_fd;
    char *cinta;
    sem_t *mutex, *sem_BC, *done_BC;
    int cp = 0;  // Contador de pares empaquetados

    // Abrir la memoria compartida existente
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0660);
    if (shm_fd == -1) {
        perror("No se pudo abrir la memoria compartida");
        exit(1);
    }

    // Mapear el área de memoria compartida
    cinta = mmap(NULL, 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (cinta == MAP_FAILED) {
        perror("No se pudo mapear la memoria");
        close(shm_fd);
        exit(1);
    }

    // Abrir semáforo mutex
    mutex = sem_open(MUTEX, 0);
    if (mutex == SEM_FAILED) {
        perror("robot3: Error al abrir semáforo mutex");
        exit(1);
    }

    // Abrir semáforo sem_BC
    sem_BC = sem_open(SEM_BC, 0);
    if (sem_BC == SEM_FAILED) {
        perror("robot3: Error al abrir semáforo sem_BC");
        exit(1);
    }

    // Abrir semáforo done_BC
    done_BC = sem_open(DONE_BC, 0);
    if (done_BC == SEM_FAILED) {
        perror("robot3: Error al abrir semáforo done_BC");
        exit(1);
    }

    // Bucle principal de trabajo
    while (1) {
        sem_wait(sem_BC);      // Espera señal para leer par BC
        sem_wait(mutex);       // Entra a región crítica

        // Leer los productos desde la cinta
        char producto[3];
        producto[0] = cinta[0];
        producto[1] = cinta[1];
        producto[2] = '\0';

        if (strcmp(producto, "BC") == 0) {
            cp++;              // Empaqueta el par BC
            sem_post(done_BC);
            cinta[0] = '-';    // Limpia la cinta
            cinta[1] = '-';
        } else if (strcmp(producto, "ZZ") == 0) {
            sem_post(mutex);   // Libera el mutex antes de salir
            break;             // Finaliza el ciclo
        }

        sem_post(mutex);       // Sale de región crítica
    }

    // Enviar cantidad de pares empaquetados por FIFO
    int fifo_fd = open(FIFO_ROBOT3, O_WRONLY);
    if (fifo_fd == -1) {
        perror("Error al abrir el FIFO robot3_fifo");
        return 1;
    }

    if (write(fifo_fd, &cp, sizeof(int)) == -1) {
        perror("Error al escribir en el FIFO robot3_fifo");
        close(fifo_fd);
        return 1;
    }

    close(fifo_fd);

    // Liberar recursos del sistema
    munmap(cinta, 2);
    close(shm_fd);
    sem_close(mutex);
    sem_close(sem_BC);
    sem_close(done_BC);

    return 0;
}
