#include <stdio.h>      // Para entrada y salida estándar
#include <stdlib.h>     // Para funciones como atoi(), exit()
#include <fcntl.h>      // Para constantes de control de archivos
#include <sys/mman.h>   // Para manejo de memoria compartida (shm_open, mmap)
#include <sys/stat.h>   // Para permisos
#include <sys/wait.h>   // Para wait()
#include <sys/types.h>  // Para tipos como pid_t
#include <unistd.h>     // Para fork(), read(), write(), close(), etc.
#include <semaphore.h>  // Para semáforos POSIX
#include <string.h>     // Para strcmp()
#include <errno.h>      // Para manejo de errores
#include <time.h>       // Para inicializar la semilla de números aleatorios

// Definiciones de nombres para memoria compartida, semáforos y FIFOs
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
    // Eliminar semáforos existentes por si quedaron colgados
    sem_unlink(MUTEX);
    sem_unlink(SEM_AB);
    sem_unlink(SEM_AC);
    sem_unlink(SEM_BC);
    sem_unlink(DONE_AB);
    sem_unlink(DONE_AC);
    sem_unlink(DONE_BC);

    // Validación de argumento N
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <N par>\n", argv[0]);
        exit(1);
    }

    int N = atoi(argv[1]);
    if (N <= 0 || N % 2 != 0) {
        fprintf(stderr, "Error: N debe ser un número par positivo\n");
        exit(1);
    }

    // Crear tubería anónima para enviar N al hijo
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    // Crear y configurar memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(char) * 2) == -1) {
        perror("ftruncate");
        exit(1);
    }

    // Mapear memoria compartida
    char *cinta = mmap(NULL, sizeof(char) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (cinta == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Crear semáforos POSIX con nombre
    sem_t *mutex = sem_open(MUTEX, O_CREAT, 0666, 1);
    sem_t *sem_AB = sem_open(SEM_AB, O_CREAT, 0666, 0);
    sem_t *sem_AC = sem_open(SEM_AC, O_CREAT, 0666, 0);
    sem_t *sem_BC = sem_open(SEM_BC, O_CREAT, 0666, 0);
    sem_t *done_AB = sem_open(DONE_AB, O_CREAT | O_EXCL, 0660, 0);
    sem_t *done_AC = sem_open(DONE_AC, O_CREAT | O_EXCL, 0660, 0);
    sem_t *done_BC = sem_open(DONE_BC, O_CREAT | O_EXCL, 0660, 0);

    if (mutex == SEM_FAILED || sem_AB == SEM_FAILED || sem_AC == SEM_FAILED ||
        sem_BC == SEM_FAILED || done_AB == SEM_FAILED || done_AC == SEM_FAILED ||
        done_BC == SEM_FAILED) {
        perror("sem_open en fabrica");
        exit(EXIT_FAILURE);
    }

    // Crear proceso productor
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Código del proceso hijo (productor)
        close(pipefd[1]);
        if (read(pipefd[0], &N, sizeof(int)) != sizeof(int)) {
            perror("read N");
            exit(1);
        }
        close(pipefd[0]);

        // Abrir memoria compartida y semáforos como consumidor
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

        if (mutex == SEM_FAILED || sem_AB == SEM_FAILED || sem_AC == SEM_FAILED ||
            sem_BC == SEM_FAILED || done_AB == SEM_FAILED || done_AC == SEM_FAILED ||
            done_BC == SEM_FAILED) {
            perror("sem_open en hijo productor");
            exit(EXIT_FAILURE);
        }

        srand(time(NULL));
        const char *pares[] = {"AB", "AC", "BC"};

        for (int i = 0; i < N; i++) {
            sem_wait(mutex);
            const char *par = pares[rand() % 3];
            cinta[0] = par[0];
            cinta[1] = par[1];
            printf("Producido: %c%c\n", cinta[0], cinta[1]);
            fflush(stdout);
            sem_post(mutex);

            if (strcmp(par, "AB") == 0) {
                sem_post(sem_AB);
                sem_wait(done_AB);
            } else if (strcmp(par, "AC") == 0) {
                sem_post(sem_AC);
                sem_wait(done_AC);
            } else if (strcmp(par, "BC") == 0) {
                sem_post(sem_BC);
                sem_wait(done_BC);
            }
        }

        // Señal de finalización: ZZ
        for (int i = 0; i < 3; i++) {
            sem_wait(mutex);
            cinta[0] = 'Z';
            cinta[1] = 'Z';
            sem_post(mutex);
            if (i == 0) sem_post(sem_AB);
            else if (i == 1) sem_post(sem_AC);
            else if (i == 2) sem_post(sem_BC);
        }

        // Cierre de recursos en hijo
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

    // Código del proceso padre (fabrica.c)
    close(pipefd[0]);
    write(pipefd[1], &N, sizeof(int));
    close(pipefd[1]);

    // Crear FIFOs para comunicarse con los robots
    mkfifo(FIFO_ROBOT1, 0666);
    mkfifo(FIFO_ROBOT2, 0666);
    mkfifo(FIFO_ROBOT3, 0666);

    // Leer conteo de empaques de cada robot
    int cp1 = 0, cp2 = 0, cp3 = 0;
    int fd_fifo;

    fd_fifo = open(FIFO_ROBOT1, O_RDONLY);
    if (fd_fifo == -1) { perror("open FIFO_ROBOT1"); exit(1); }
    if (read(fd_fifo, &cp1, sizeof(int)) != sizeof(int)) { perror("read FIFO_ROBOT1"); exit(1); }
    close(fd_fifo);

    fd_fifo = open(FIFO_ROBOT2, O_RDONLY);
    if (fd_fifo == -1) { perror("open FIFO_ROBOT2"); exit(1); }
    if (read(fd_fifo, &cp2, sizeof(int)) != sizeof(int)) { perror("read FIFO_ROBOT2"); exit(1); }
    close(fd_fifo);

    fd_fifo = open(FIFO_ROBOT3, O_RDONLY);
    if (fd_fifo == -1) { perror("open FIFO_ROBOT3"); exit(1); }
    if (read(fd_fifo, &cp3, sizeof(int)) != sizeof(int)) { perror("read FIFO_ROBOT3"); exit(1); }
    close(fd_fifo);

    // Mostrar resultado de cada robot
    printf("Robot 1 empacó %d pares.\n", cp1);
    printf("Robot 2 empacó %d pares.\n", cp2);
    printf("Robot 3 empacó %d pares.\n", cp3);

    // Liberación de recursos en padre
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
    wait(NULL); // Esperar hijo

    return 0;
}
