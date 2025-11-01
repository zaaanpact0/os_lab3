gcc -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L -o server server.c -lrt -pthread

gcc -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L -o child1 child1.c -lrt -pthread

gcc -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L -o child2 child2.c -lrt -pthread
