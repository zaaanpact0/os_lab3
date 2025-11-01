#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024
#define SHM_SIZE (BUFFER_SIZE * 3)

typedef struct {
    char data[BUFFER_SIZE];
    size_t size;
    int stop;
} shared_data_t;

void to_lower_case(char* str, size_t size) {
    for (size_t i = 0; i < size && str[i] != '\0'; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] + ('a' - 'A');
        }
    }
}

void print_error(const char* msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_error("Usage: child1 <shm_name> <sem_name>");
        exit(EXIT_FAILURE);
    }

    char* shm_name = argv[1];
    char* sem_name = argv[2];

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        print_error("Error: child1 failed to open shared memory");
        exit(EXIT_FAILURE);
    }

    shared_data_t *shm_data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
        print_error("Error: child1 failed to map shared memory");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
    close(shm_fd);

    sem_t *semaphore = sem_open(sem_name, 0);
    if (semaphore == SEM_FAILED) {
        print_error("Error: child1 failed to open semaphore");
        munmap(shm_data, SHM_SIZE);
        exit(EXIT_FAILURE);
    }

    while (1) {
        sem_wait(semaphore);
        
        if (shm_data[0].stop) {
            sem_post(semaphore);
            break;
        }

        if (shm_data[0].size > 0) {
            memcpy(shm_data[1].data, shm_data[0].data, shm_data[0].size);
            shm_data[1].size = shm_data[0].size;
            
            to_lower_case(shm_data[1].data, shm_data[1].size);
            
            shm_data[0].size = 0;
        }
        
        sem_post(semaphore);
        sleep(1);
    }

    sem_close(semaphore);
    munmap(shm_data, SHM_SIZE);

    return 0;
}