#include<stdio.h>
#include<syslog.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>


int main(int argc, char* argv[]){
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
    if(argc != 3){
        syslog(LOG_ERR,"Usage: %s <writefile> <writestr>", argv[0] );
        return 1;
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ssize_t ret = write(fd, argv[2], strlen(argv[2]));
    if(ret == -1){
        syslog(LOG_ERR, "Could not write to file %s", argv[2]);
        return 1;
    }
    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

    closelog();
}
