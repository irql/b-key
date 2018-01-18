#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>

unsigned char *memory_alloc(
    int page_count
) {
    if(page_count > 0) {
        unsigned int page_size = sysconf(_SC_PAGE_SIZE);
        unsigned char *region = mmap(NULL, page_count * page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(region == MAP_FAILED) {
            printf("%s\n", strerror(errno));
            return NULL;
        }
        else {
            return region;
        }
    }
    
    return NULL;
}
