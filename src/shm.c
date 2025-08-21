#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "shm.h"

static int flags_to_prot(int oflags) {
    return (oflags & O_RDWR) ? (PROT_READ | PROT_WRITE) : PROT_READ;
}

void* shm_create(const char* name, size_t size, int oflags) {
    int fd = shm_open(name, oflags | O_CREAT, 0666);
    if (fd == -1) {
        fprintf(stderr, "shm_open(%s, create, oflags=0x%x) fallo: %s\n",
                name, oflags, strerror(errno));
        return NULL;
    }

    if (ftruncate(fd, (off_t)size) == -1) {
        fprintf(stderr, "ftruncate(%s, %zu) fallo: %s\n", name, size, strerror(errno));
        close(fd);
        return NULL;
    }

    int prot = flags_to_prot(oflags);
    void* addr = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "mmap(%s) fallo: %s\n", name, strerror(errno));
        close(fd);
        return NULL;
    }

    close(fd);
    return addr;
}

void* shm_connect(const char* name, size_t size, int oflags) {
    int fd = shm_open(name, oflags, 0);
    if (fd == -1) {
        fprintf(stderr, "shm_open(%s, oflags=0x%x) fallo: %s\n", name, oflags, strerror(errno));
        return NULL;
    }

    int prot = flags_to_prot(oflags);
    void* addr = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "mmap(%s) fallo: %s\n", name, strerror(errno));
        close(fd);
        return NULL;
    }

    close(fd);
    return addr;
}

int shm_unmap(void* addr, size_t size) {
    return munmap(addr, size);
}

int shm_delete(const char* name) {
    return shm_unlink(name);
}