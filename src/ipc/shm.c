// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "shm.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static int flags_to_prot(int oflags) {
	return (oflags & O_RDWR) ? (PROT_READ | PROT_WRITE) : PROT_READ;
}

void *shm_create(const char *name, size_t size, int oflags) {
	if (size <= 0) {
		fprintf(stderr, "Error: Attempt to create a shared memory with an invalid size.\n");
		return NULL;
	}

	int fd = shm_open(name, oflags | O_CREAT, 0666);
	if (fd == -1) {
		perror("shm_open");
		return NULL;
	}

	if (ftruncate(fd, (off_t) size) == -1) {
		perror("ftruncate");
		close(fd);
		return NULL;
	}

	int prot = flags_to_prot(oflags);
	void *addr = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return NULL;
	}

	close(fd);
	return addr;
}

void *shm_connect(const char *name, size_t size, int oflags) {
	if (size <= 0) {
		fprintf(stderr, "Error: Attempt to connect to a shared memory with an invalid size.\n");
		return NULL;
	}

	int fd = shm_open(name, oflags, 0);
	if (fd == -1) {
		perror("shm_open");
		return NULL;
	}

	int prot = flags_to_prot(oflags);
	void *addr = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return NULL;
	}

	close(fd);
	return addr;
}

int shm_unmap(void *addr, size_t size) {
	if (addr == NULL) {
		fprintf(stderr, "Error: Attempt to unmap a NULL address.\n");
		return -1;
	}
	if (size <= 0) {
		fprintf(stderr, "Error: Attempt to unmap invalid size.\n");
		return -1;
	}

	if (munmap(addr, size) == -1) {
		perror("munmap");
		return -1;
	}
	return 0;
}

int shm_delete(const char *name) {
	if (name == NULL || name[0] == '\0') {
		fprintf(stderr, "Error: Attempt to delete a shared memory with a NULL name.\n");
		return -1;
	}

	if (shm_unlink(name) == -1) {
		perror("shm_unlink");
		return -1;
	}

	return 0;
}
