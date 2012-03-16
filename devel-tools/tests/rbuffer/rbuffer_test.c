#include "../../../src/queues.h" // We expect that ring buffer API does not change
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define THREAD_NUM 2
#define BUFFER_SIZE 10
#define WRITE_COUNT 100
#define READ_COUNT WRITE_COUNT

struct ring_buffer *rb;
int delays[THREAD_NUM] = {0, 500};

void *reader_thread(void *arg)
{
	unsigned int index = -1;
	int num = *((int*) arg);
	struct ipfix_message *msg;

	printf("Starting thread %i with delay %i\n", num, delays[num]);

	for (int i=0; i<READ_COUNT; i++) {
		msg = rbuffer_read(rb, &index);

		/* give the data a chance to disappear */
		usleep(delays[num]);

		/* lock the rbuffer so that the error output is consistent */
		if (pthread_mutex_lock (&(rb->mutex)) != 0) {
			printf("Mutex lock failed (%s:%d)", __FILE__, __LINE__);
			return (NULL);
		}

		if (msg == NULL || msg->pkt_header == NULL || msg->pkt_header->observation_domain_id != 1000) {
			printf("Thread num: %i iteration: %i read from index: %i\n", num, i, index);
			printf("Error: data freed too early\n");
			printf("buffer size: %i buffer count: %i \n\n", rb->size, rb->count);
		}

		if (pthread_mutex_unlock(&(rb->mutex)) != 0) {
			printf("Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
			return (NULL);
		}

		rbuffer_remove_reference(rb, index, 1);

		index = (index+1) % BUFFER_SIZE;
	}

	return NULL;
}

int main()
{
	rb = rbuffer_init(BUFFER_SIZE);

	pthread_t threads[THREAD_NUM];
	int idarray[THREAD_NUM];

	for (int i = 0; i < THREAD_NUM; i++) {
		idarray[i] = i;
		pthread_create(&threads[i], NULL, reader_thread, &idarray[i]);
	}

	for (int i=0; i<WRITE_COUNT; i++) {

		struct ipfix_message *record = malloc(sizeof(struct ipfix_message));
		record->pkt_header = malloc(sizeof(struct ipfix_header));
		record->pkt_header->observation_domain_id = 1000;
		rbuffer_write(rb, record, THREAD_NUM);
//		usleep(600);
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(threads[i], NULL);
	}

	rbuffer_free(rb);

	return 0;
}
