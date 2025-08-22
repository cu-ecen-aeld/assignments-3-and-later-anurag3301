#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "aesd_ioctl.h"

int main(int argc, char *argv[])
{
    int fd;
    struct aesd_seekto seekto;
    char buffer[1024];
    ssize_t bytes_read;
    
    if (argc != 3) {
        printf("Usage: %s <write_cmd> <write_cmd_offset>\n", argv[0]);
        printf("Example: %s 1 2\n", argv[0]);
        return 1;
    }
    
    seekto.write_cmd = atoi(argv[1]);
    seekto.write_cmd_offset = atoi(argv[2]);
    
    printf("Testing IOCTL with write_cmd=%u, write_cmd_offset=%u\n", 
           seekto.write_cmd, seekto.write_cmd_offset);
    
    // Open the device
    fd = open("/dev/aesdchar", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/aesdchar");
        return 1;
    }
    
    // Perform the ioctl
    if (ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto) < 0) {
        perror("IOCTL failed");
        close(fd);
        return 1;
    }
    
    printf("IOCTL successful! Current file position after seek:\n");
    
    // Read from current position
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read >= 0) {
        buffer[bytes_read] = '\0';
        printf("Read %zd bytes: '%s'\n", bytes_read, buffer);
    } else {
        perror("Read failed");
    }
    
    close(fd);
    return 0;
}
