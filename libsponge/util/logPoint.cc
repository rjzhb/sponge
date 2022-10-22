#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define MAX_LEN 1024

bool debug_mode;

// 使用方法同 printf
void lprintf(const char *fmt, ...) {
    static bool print_time = true;  // 是否要打印时间: 当 debug_mode 为真，且上一次是换行符结尾。
    char message[MAX_LEN];
    // 当前时间.
    time_t timer = time(NULL);
    strftime(message, 23, "[%Y-%m-%d %H:%M:%S] ", localtime(&timer));
    va_list args;
    va_start(args, fmt);
    vsnprintf(message + 22, MAX_LEN - 22, fmt, args);
    va_end(args);

    if (debug_mode) {
        printf("%s", message + 22);
    }

    int fd = open("error.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        perror("open (log)");
    } else {
        if (print_time == false) {
            if (write(fd, message + 22, strlen(message + 22)) == -1)
                perror("lprintf");
        } else {
            if (write(fd, message, strlen(message)) == -1)
                perror("lprintf");
        }

        print_time = (message[strlen(message) - 1] == '\n');
        close(fd);
    }
}
