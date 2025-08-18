#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#define PORT 9000
#define BACKLOG 10
#if USE_AESD_CHAR_DEVICE
#define FILENAME "/dev/aesdchar"
#else
#define FILENAME "/var/tmp/aesdsocketdata"
#endif

int sockfd=-1, client_fd=-1;
FILE *client_stream=NULL, *aesd_outfile=NULL;
pthread_mutex_t file_mutex;

void cleanup(bool all) {
    if (client_stream != NULL) {
        fclose(client_stream);
        client_stream = NULL;
    }

    if (aesd_outfile != NULL) {
        fclose(aesd_outfile);
        aesd_outfile = NULL;
    }

    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;
    }

    if(all){
        if (sockfd != -1) {
            close(sockfd);
            sockfd = -1;
        }

        remove(FILENAME);
        closelog(); 
    }
}

void* timer_thread_func(void* arg){
    while(1){
        sleep(10);
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        if (timeinfo == NULL) {
            cleanup(true);            
        }

        char timestamp_str[128];
        strftime(timestamp_str, sizeof(timestamp_str), "timestamp: %a, %d %b %Y %T %z\n", timeinfo);

        pthread_mutex_lock(&file_mutex);
        aesd_outfile = fopen(FILENAME, "a+");
        if (aesd_outfile == NULL) {
            syslog(LOG_ERR, "Could not make aesd outfile stream: %s", strerror(errno));
            cleanup(true);
        }
        fprintf(aesd_outfile, "%s", timestamp_str);
        cleanup(false);
        pthread_mutex_unlock(&file_mutex);
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        cleanup(true);
        exit(0);
    }
}

void setup_signals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void daemonize() {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("second fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (chdir("/") < 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

int main(int argc, char *argv[]) {
    setup_signals();

    if(argc == 2 && strcmp(argv[1], "-d")==0){
        printf("Here\n");
        daemonize(); 
    }

#if !USE_AESD_CHAR_DEVICE
    pthread_t timer_thread;

    if (pthread_create(&timer_thread, NULL, timer_thread_func, NULL) != 0) {
        cleanup(true);
        return 1;
    }
#endif

    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        cleanup(true);
        return -1;
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        syslog(LOG_ERR, "setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        cleanup(true);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        cleanup(true);
        return -1;
    }

    // Listen
    if (listen(sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        cleanup(true);
        return -1;
    }

    while(1){
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            cleanup(true);
            return -1;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        client_stream = fdopen(client_fd, "w+");
        if (client_stream == NULL) {
            syslog(LOG_ERR, "Could not make client stream: %s", strerror(errno));
            cleanup(true);
            return -1;
        }

        pthread_mutex_lock(&file_mutex);
        aesd_outfile = fopen(FILENAME, "a+");
        if (aesd_outfile == NULL) {
            syslog(LOG_ERR, "Could not make aesd outfile stream: %s", strerror(errno));
            cleanup(true);
            return -1;
        }

        
        char *line = NULL;
        size_t len = 0;
        getline(&line, &len, client_stream);
        fprintf(aesd_outfile, "%s", line);
        fflush(aesd_outfile);

        fseek(aesd_outfile, 0, SEEK_SET);

        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), aesd_outfile) != NULL) {
            fputs(buffer, client_stream);
        }
        fflush(client_stream);

        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
        free(line);
        cleanup(false);
        pthread_mutex_unlock(&file_mutex);

    }

    cleanup(true);

    return 0;
}
