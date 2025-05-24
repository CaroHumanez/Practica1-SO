#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define SHM_NAME "/cinta_shm"
#define SEM_PROD "/sem_prod"
#define SEM_CONS "/sem_cons"
#define FIFO_ROBOT1 "robot1_fifo"
#define FIFO_ROBOT2 "robot2_fifo"
#define FIFO_ROBOT3 "robot3_fifo"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <N par>\n", argv[0]);
        exit(1);
    }

    int N = atoi(argv[1]);
    if (N <= 0 || N % 2 != 0) {
        fprintf(stderr, "Error: N debe ser un número par positivo\n");
        exit(1);
    }

    // Crear tubería anónima
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    // Crear memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(char) * 2) == -1) {
        perror("ftruncate");
        exit(1);
    }

    char *cinta = mmap(NULL, sizeof(char) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (cinta == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Crear semáforos
    sem_t *sem_prod = sem_open(SEM_PROD, O_CREAT, 0666, 1);
    sem_t *sem_cons = sem_open(SEM_CONS, O_CREAT, 0666, 0);
    if (sem_prod == SEM_FAILED || sem_cons == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Crear proceso hijo (productor)
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Proceso hijo: productor
        close(pipefd[1]); // cerrar escritura
        dup2(pipefd[0], STDIN_FILENO); // redirigir entrada
        close(pipefd[0]);

        int N;
        if (read(STDIN_FILENO, &N, sizeof(int)) != sizeof(int)) {
            perror("read N");
            exit(1);
        }

        int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open hijo");
            exit(1);
        }

        char *cinta = mmap(NULL, sizeof(char) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (cinta == MAP_FAILED) {
            perror("mmap hijo");
            exit(1);
        }

        sem_t *sem_prod = sem_open(SEM_PROD, 0);
        sem_t *sem_cons = sem_open(SEM_CONS, 0);
        if (sem_prod == SEM_FAILED || sem_cons == SEM_FAILED) {
            perror("sem_open hijo");
            exit(1);
        }

        srand(time(NULL));
        const char *pares[] = {"AB", "AC", "BC"};

        for (int i = 0; i < N; i++) {
            sem_wait(sem_prod);
            const char *par = pares[rand() % 3];
            cinta[0] = par[0];
            cinta[1] = par[1];
            printf("Producido: %c%c\n", cinta[0], cinta[1]);
            fflush(stdout);
            sem_post(sem_cons);
        }

        for (int i = 0; i < 3; i++) {  // uno por robot
            sem_wait(sem_prod);
            cinta[0] = 'Z';
            cinta[1] = 'Z';
            sem_post(sem_cons);
        }

        munmap(cinta, sizeof(char) * 2);
        close(shm_fd);
        sem_close(sem_prod);
        sem_close(sem_cons);
        exit(0);
    }

    // Proceso padre
    close(pipefd[0]); // cerrar lectura
    write(pipefd[1], &N, sizeof(int));
    close(pipefd[1]);

    // Crear FIFOs
    mkfifo(FIFO_ROBOT1, 0666);
    mkfifo(FIFO_ROBOT2, 0666);
    mkfifo(FIFO_ROBOT3, 0666);

    int cp1 = 0, cp2 = 0, cp3 = 0;
    int fd_fifo;

    fd_fifo = open(FIFO_ROBOT1, O_RDONLY);
    if (fd_fifo == -1) { perror("open FIFO_ROBOT1"); exit(1); }
    if (read(fd_fifo, &cp1, sizeof(int)) != sizeof(int)) {
        perror("read FIFO_ROBOT1");
        exit(1);
    }
    close(fd_fifo);

    fd_fifo = open(FIFO_ROBOT2, O_RDONLY);
    if (fd_fifo == -1) { perror("open FIFO_ROBOT2"); exit(1); }
    if (read(fd_fifo, &cp2, sizeof(int)) != sizeof(int)) {
        perror("read FIFO_ROBOT2");
        exit(1);
    }
    close(fd_fifo);

    fd_fifo = open(FIFO_ROBOT3, O_RDONLY);
    if (fd_fifo == -1) { perror("open FIFO_ROBOT3"); exit(1); }
    if (read(fd_fifo, &cp3, sizeof(int)) != sizeof(int)) {
        perror("read FIFO_ROBOT3");
        exit(1);
    }
    close(fd_fifo);

    printf("Robot 1 empacó %d pares.\n", cp1);
    printf("Robot 2 empacó %d pares.\n", cp2);
    printf("Robot 3 empacó %d pares.\n", cp3);

    sem_close(sem_prod);
    sem_close(sem_cons);
    sem_unlink(SEM_PROD);
    sem_unlink(SEM_CONS);
    munmap(cinta, sizeof(char) * 2);
    shm_unlink(SHM_NAME);
    unlink(FIFO_ROBOT1);
    unlink(FIFO_ROBOT2);
    unlink(FIFO_ROBOT3);
    wait(NULL);

    return 0;
}