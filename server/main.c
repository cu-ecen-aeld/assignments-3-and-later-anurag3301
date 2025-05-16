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

#define PORT 9000
#define BACKLOG 10
int sockfd=-1, client_fd=-1;
FILE *client_stream=NULL, *aesd_outfile=NULL;

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

        remove("/var/tmp/aesdsocketdata");
        closelog(); 
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        cleanup(true);
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

        aesd_outfile = fopen("/var/tmp/aesdsocketdata", "a+");
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
    }

    cleanup(true);

    return 0;
}
