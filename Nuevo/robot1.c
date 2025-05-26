// robot1.c - Proceso que empaqueta pares AB desde la memoria compartida

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
#define SEM_AB "/sem_AB"
#define DONE_AB "/done_AB"
#define FIFO_ROBOT1 "robot1_fifo"

int main() {
    int shm_fd;
    char *cinta;
    sem_t *mutex, *sem_AB, *done_AB;
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
        perror("robot1: Error al abrir semáforo mutex");
        exit(1);
    }

    // Abrir semáforo sem_AB
    sem_AB = sem_open(SEM_AB, 0);
    if (sem_AB == SEM_FAILED) {
        perror("robot1: Error al abrir semáforo sem_AB");
        exit(1);
    }

    // Abrir semáforo done_AB
    done_AB = sem_open(DONE_AB, 0);
    if (done_AB == SEM_FAILED) {
        perror("robot1: Error al abrir semáforo done_AB");
        exit(1);
    }

    // Bucle principal de trabajo
    while (1) {
        sem_wait(sem_AB);      // Espera a que se coloque un par AB
        sem_wait(mutex);       // Entra a región crítica

        // Leer productos desde la cinta (memoria compartida)
        char producto[3];
        producto[0] = cinta[0];
        producto[1] = cinta[1];
        producto[2] = '\0';

        if (strcmp(producto, "AB") == 0) {
            cp++;              // Empaqueta el par AB
            sem_post(done_AB);
            cinta[0] = '-';    // Limpia la cinta
            cinta[1] = '-';
        } else if (strcmp(producto, "ZZ") == 0) {
            sem_post(mutex);   // Libera el mutex antes de terminar
            break;             // Finaliza el ciclo
        }

        sem_post(mutex);       // Sale de región crítica
    }

    // Envía cantidad de pares empaquetados al proceso padre por FIFO
    int fifo_fd = open(FIFO_ROBOT1, O_WRONLY);
    if (fifo_fd == -1) {
        perror("Error al abrir el FIFO robot1_fifo");
        return 1;
    }

    if (write(fifo_fd, &cp, sizeof(int)) == -1) {
        perror("Error al escribir en el FIFO robot1_fifo");
        close(fifo_fd);
        return 1;
    }

    close(fifo_fd);

    // Libera recursos
    munmap(cinta, 2);
    close(shm_fd);
    sem_close(mutex);
    sem_close(sem_AB);
    sem_close(done_AB);

    return 0;
}
