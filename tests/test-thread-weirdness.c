#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void * b_task(void * ctx) {
	(void)ctx;

	fprintf(stderr, "This is the inner thread (pid=%d tid=%d ppid=%d)\n", getpid(), gettid(), getppid());


	if (!fork()) {
		fprintf(stderr, "And this is a forked process (pid=%d tid=%d ppid=%d)\n", getpid(), gettid(), getppid());

		while (1) {
			usleep(10000);
		}

		exit(127);
	}

	while (1) {
		usleep(10000);
	}

	return NULL;
}

void * a_task(void * ctx) {
	(void)ctx;

	fprintf(stderr, "This is the first thread, (pid=%d tid=%d ppid=%d)\n", getpid(), gettid(), getppid());

	pthread_t b;

	pthread_create(&b, NULL, b_task, NULL);

	while (1) {
		usleep(10000);
	}


	return NULL;
}


int main(int argc, char * argv[]) {
	fprintf(stderr, "This is the main thread thread, (pid=%d tid=%d ppid=%d)\n", getpid(), gettid(), getppid());

	pthread_t a;

	pthread_create(&a, NULL, a_task, NULL);

	pthread_join(a, NULL);

}
