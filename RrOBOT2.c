//ROBOT1
// robot1.c - Abre la memoria compartida "cinta_shm"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h> 
#include <unistd.h>     
#include <sys/stat.h>       

int main() {
    //variables y descriptor
    int shm_fd;               
    char *cinta;
    sem_t *sem_prod, *sem_cons;
    int cp = 0;


    // 1.abrir la memoria compartida (creada en fabrica.c)
    shm_fd = shm_open("/cinta_shm", O_RDWR, 0660);
    if (shm_fd == -1) {
        perror("No se pudo abrir la memoria compartida");
        exit(1);
    }

    // 2.mapear la memoria
    cinta = mmap(NULL, 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (cinta == MAP_FAILED) {
        perror("No se pudo mapear la memoria");
        close(shm_fd);
        exit(1);
    }

    // 3.abrir semáforo del productor
    sem_prod = sem_open("/sem_prod", 0);
    if (sem_prod == SEM_FAILED) {
        perror("robot1: Error al abrir semáforo /sem_prod");
        exit(1);
    }

    // 4.abrir semáforo del consumo(empaquetado)
    sem_cons = sem_open("/sem_cons", 0);
    if (sem_cons == SEM_FAILED) {
        perror("robot1: Error al abrir semáforo /sem_cons");
        exit(1);
    }

    //5.ciclo que evalua la cinta
    while (1) {
        sem_wait(sem_cons); // Esperar permiso para leer lo que productor pone en la cinta

        // leer el par de letras
        char producto[3];
        producto[0] = cinta[0];
        producto[1] = cinta[1];
        producto[2] = '\0'; //caracter especial de fin

        // verificar qué el producto sea AC
        if (strcmp(producto, "AC") == 0) {
            printf("Robot 1 empaqueta productos AB\n");
            cp++;
            //aqui se "elimina" el par ab de la cinta
            cinta[0] = '-';
            cinta[1] = '-';
        } else if (strcmp(producto, "ZZ") == 0) {
            printf("Robot 1 recibió el zz de fin\n");
            break; // Salir del bucle
        }

        // Liberar la cinta para el siguiente producto
        sem_post(sem_prod);
    }

    // 6.abrir el FIFO1  para write el cp
    int fifo_fd = open("robot1_fifo", O_WRONLY);
        
    if (fifo_fd == -1) {
        perror("Error al abrir el FIFO robot1_fifo");
        return 1; // Salida con error
    }

    // 7.escribir el valor de cp en la tuberia
    if (write(fifo_fd, &cp, sizeof(int)) == -1) {
        perror("Error al escribir en el FIFO robot1_fifo");
        close(fifo_fd);
        return 1;
    }


    // Tengo que iberar los recursos al final asi: no se si piuedo dejar todo aqui al final
    munmap(cinta, 2);
    close(shm_fd);

    sem_close(sem_prod);
    sem_close(sem_cons);

    close(fifo_fd);

    return 0;
}
