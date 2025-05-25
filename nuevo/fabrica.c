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
#define MUTEX "/mutex"
#define SEM_AB "/sem_AB"
#define SEM_AC "/sem_AC"
#define SEM_BC "/sem_BC"
#define DONE_AB "/done_AB"
#define DONE_AC "/done_AC"
#define DONE_BC "/done_BC"
#define FIFO_ROBOT1 "robot1_fifo"
#define FIFO_ROBOT2 "robot2_fifo"
#define FIFO_ROBOT3 "robot3_fifo"

int main(int argc, char *argv[]) {

    sem_unlink(MUTEX);
    sem_unlink(SEM_AB);
    sem_unlink(SEM_AC);
    sem_unlink(SEM_BC);
    sem_unlink(DONE_AB);
    sem_unlink(DONE_AC);
    sem_unlink(DONE_BC);

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
    sem_t *mutex = sem_open(MUTEX, O_CREAT, 0666, 1);
    sem_t *sem_AB = sem_open(SEM_AB, O_CREAT, 0666, 0);
    sem_t *sem_AC = sem_open(SEM_AC, O_CREAT, 0666, 0);
    sem_t *sem_BC = sem_open(SEM_BC, O_CREAT, 0666, 0);

    sem_t *done_AB = sem_open(DONE_AB, O_CREAT | O_EXCL, 0660, 0);
    sem_t *done_AC = sem_open(DONE_AC, O_CREAT | O_EXCL, 0660, 0);
    sem_t *done_BC = sem_open(DONE_BC, O_CREAT | O_EXCL, 0660, 0);

  if (mutex == SEM_FAILED || sem_AB == SEM_FAILED || sem_AC == SEM_FAILED || sem_BC == SEM_FAILED ||
    done_AB == SEM_FAILED || done_AC == SEM_FAILED || done_BC == SEM_FAILED) {
         perror("sem_open en fabrica");
         exit(EXIT_FAILURE);
  }   

    // Crear proceso hijo (productor)
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Proceso hijo

        //Leer el numero n desde la tuberia
        close(pipefd[1]); // Cerrar extremo de escritura
        int N;
        if (read(pipefd[0], &N, sizeof(int)) != sizeof(int)) {
            perror("read N");
            exit(1);
        }
        close(pipefd[0]);

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

        sem_t *mutex = sem_open(MUTEX, 0);
        sem_t *sem_AB = sem_open(SEM_AB, 0);
        sem_t *sem_AC = sem_open(SEM_AC, 0);
        sem_t *sem_BC = sem_open(SEM_BC, 0);
        sem_t *done_AB = sem_open(DONE_AB, 0);
        sem_t *done_AC = sem_open(DONE_AC, 0);
        sem_t *done_BC = sem_open(DONE_BC, 0);

        if (mutex == SEM_FAILED || sem_AB == SEM_FAILED || sem_AC == SEM_FAILED || sem_BC == SEM_FAILED ||
            done_AB == SEM_FAILED || done_AC == SEM_FAILED || done_BC == SEM_FAILED) {
            perror("sem_open en hijo productor");
            exit(EXIT_FAILURE);
        }

        srand(time(NULL));
        const char *pares[] = {"AB", "AC", "BC"};

        for (int i = 0; i < N; i++) {
            printf("Inicia ejecucion %d\n", i);
            // Acceso exclusivo a la memoria compartida
            sem_wait(mutex);

            const char *par = pares[rand() % 3];
            cinta[0] = par[0];
            cinta[1] = par[1];
            printf("Producido: %c%c\n", cinta[0], cinta[1]);
            fflush(stdout);

            // Liberar el mutex para que otros puedan acceder
            sem_post(mutex);

            // Luego activamos el semáforo según el par generado
            if (par[0] == 'A' && par[1] == 'B') {
                sem_post(sem_AB);
                sem_wait(done_AB);
            } else if (par[0] == 'A' && par[1] == 'C') {
                sem_post(sem_AC);
                sem_wait(done_AC);
            } else if (par[0] == 'B' && par[1] == 'C') {
                printf("envia bc");
                sem_post(sem_BC);
                sem_wait(done_BC);
            }

            
        }

        // Señal de terminación para cada robot: producir par 'ZZ'
        for (int i = 0; i < 3; i++) {
            sem_wait(mutex);
            cinta[0] = 'Z';
            cinta[1] = 'Z';
            sem_post(mutex);

            if (i == 0) sem_post(sem_AB);
            else if (i == 1) sem_post(sem_AC);
            else if (i == 2) sem_post(sem_BC);
        }

        // Eliminar recursos de hijo
        munmap(cinta, sizeof(char) * 2);
        close(shm_fd);
        sem_close(mutex);
        sem_close(sem_AB);
        sem_close(sem_AC);
        sem_close(sem_BC);
        sem_close(done_AB);
        sem_close(done_AC);
        sem_close(done_BC);
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
    if (fd_fifo == -1) { 
        perror("open FIFO_ROBOT1"); exit(1); }
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

    // Cierre y eliminación semáforos
    sem_close(mutex);
    sem_close(sem_AB);
    sem_close(sem_AC);
    sem_close(sem_BC);

    sem_close(done_AB);
    sem_close(done_AC);
    sem_close(done_BC);


    munmap(cinta, sizeof(char) * 2);
    shm_unlink(SHM_NAME);
    unlink(FIFO_ROBOT1);
    unlink(FIFO_ROBOT2);
    unlink(FIFO_ROBOT3);
    
    wait(NULL);

    return 0;
}