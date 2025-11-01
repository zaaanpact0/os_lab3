#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define SHM_SIZE (BUFFER_SIZE * 3)

typedef struct {
    char data[BUFFER_SIZE];
    size_t size;
    int stop;
} shared_data_t;

shared_data_t *shm_data;
char shm_name[64];
char sem_name[64];

void cleanup() {
    if (shm_data != MAP_FAILED && shm_data != NULL) {
        munmap(shm_data, SHM_SIZE);
    }
    shm_unlink(shm_name);
    sem_unlink(sem_name);
}

void handle_signal(int sig) {
    (void)sig;
    cleanup();
    exit(0);
}

void print_error(const char* msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
}

int main() {
    srand(time(NULL));
    snprintf(shm_name, sizeof(shm_name), "/lab_shm_%d", rand());
    snprintf(sem_name, sizeof(sem_name), "/lab_sem_%d", rand());

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        print_error("Error: Failed to create shared memory");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        print_error("Error: Failed to set shared memory size");
        close(shm_fd);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }

    shm_data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
        print_error("Error: Failed to map shared memory");
        close(shm_fd);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }
    close(shm_fd);

    memset(shm_data, 0, SHM_SIZE);
    shm_data[0].stop = 0;
    shm_data[1].stop = 0;
    shm_data[2].stop = 0;

    sem_t *semaphore = sem_open(sem_name, O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        print_error("Error: Failed to create semaphore");
        cleanup();
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    pid_t child1 = fork();
    if (child1 == -1) {
        print_error("Error: Failed to spawn child1 process");
        cleanup();
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) {
        char shm_name_arg[64], sem_name_arg[64];
        snprintf(shm_name_arg, sizeof(shm_name_arg), "%s", shm_name);
        snprintf(sem_name_arg, sizeof(sem_name_arg), "%s", sem_name);
        
        execl("./child1", "child1", shm_name_arg, sem_name_arg, (char*)NULL);
        print_error("Error: child1 exec failed");
        exit(EXIT_FAILURE);
    }

    pid_t child2 = fork();
    if (child2 == -1) {
        print_error("Error: Failed to spawn child2 process");
        kill(child1, SIGTERM);
        cleanup();
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) {
        char shm_name_arg[64], sem_name_arg[64];
        snprintf(shm_name_arg, sizeof(shm_name_arg), "%s", shm_name);
        snprintf(sem_name_arg, sizeof(sem_name_arg), "%s", sem_name);
        
        execl("./child2", "child2", shm_name_arg, sem_name_arg, (char*)NULL);
        print_error("Error: child2 exec failed");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    const char prompt[] = "Enter string (or 'quit' to exit): ";
    const char result_prefix[] = "Result: ";

    while (1) {
        write(STDOUT_FILENO, prompt, strlen(prompt));

        bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            break;
        }

        buffer[bytes_read] = '\0';

        if (strncmp(buffer, "quit\n", 5) == 0) {
            break;
        }

        sem_wait(semaphore);
        memcpy(shm_data[0].data, buffer, bytes_read);
        shm_data[0].size = bytes_read;
        shm_data[0].stop = 0;
        sem_post(semaphore);

        sleep(1);
        
        sem_wait(semaphore);
        if (shm_data[2].size > 0) {
            write(STDOUT_FILENO, result_prefix, strlen(result_prefix));
            write(STDOUT_FILENO, shm_data[2].data, shm_data[2].size);
            shm_data[2].size = 0;
        }
        sem_post(semaphore);
    }

    sem_wait(semaphore);
    shm_data[0].stop = 1;
    shm_data[1].stop = 1;
    shm_data[2].stop = 1;
    sem_post(semaphore);

    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    sem_close(semaphore);
    cleanup();

    return 0;
}