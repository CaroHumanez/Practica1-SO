#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <SHM_NAME> <SEM_PROD> <SEM_CONS>\n", argv[0]);
        exit(1);
    }

    char *shm_name = argv[1];
    char *sem_prod_name = argv[2];
    char *sem_cons_name = argv[3];

    // Leer N desde stdin (pipe redirigido)
    int N;
    if (read(STDIN_FILENO, &N, sizeof(int)) != sizeof(int)) {
        perror("read N");
        exit(1);
    }

    // Conectarse a memoria compartida
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    char *cinta = mmap(NULL, sizeof(char) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (cinta == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Abrir semáforos
    sem_t *sem_prod = sem_open(sem_prod_name, 0);
    sem_t *sem_cons = sem_open(sem_cons_name, 0);
    if (sem_prod == SEM_FAILED || sem_cons == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Inicializar aleatoriedad
    srand(time(NULL));
    const char *pares[] = {"AB", "AC", "BC"};

    // Producción de pares
    for (int i = 0; i < N; i++) {
        sem_wait(sem_prod); // Esperar turno de escritura
        const char *par = pares[rand() % 3];
        cinta[0] = par[0];
        cinta[1] = par[1];
        printf("Producido: %c%c\n", cinta[0], cinta[1]);
        sem_post(sem_cons); // Avisar a robots
    }

    // Enviar testigo de fin "ZZ"
    sem_wait(sem_prod);
    cinta[0] = 'Z';
    cinta[1] = 'Z';
    sem_post(sem_cons);

    // Limpiar
    munmap(cinta, sizeof(char) * 2);
    sem_close(sem_prod);
    sem_close(sem_cons);

    return 0;
}
