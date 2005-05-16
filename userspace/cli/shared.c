#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>

#include "shared.h"

extern int errno;

char *key_file_path = "/tmp/cli";
struct cli_config *cfg;
int shmid;
int semid;

void cfg_init_data(void) {
	memset(cfg, 0, sizeof(struct cli_config));
}

int cfg_init(void) {
	key_t key;
	int fd;
	int init_data = 0;

	fd = creat(key_file_path, 0600);
	if(fd == -1)
		return -1;
	close(fd);
	key = ftok(key_file_path, 's');
	if(key == -1)
		return -2;
	if((shmid = shmget(key, sizeof(struct cli_config), 0600)) == -1) {
		init_data = 1;
		shmid = shmget(key, sizeof(struct cli_config), IPC_CREAT | 0600);
		if(shmid == -1)
			return -3;
	}
	cfg = shmat(shmid, NULL, 0);
	if((int)cfg == -1)
		return -4;
	//FIXME create sema
	if(init_data)
		cfg_init_data();
	return 0;
}

int cfg_lock(void) {
	//FIXME lock sema
	return 0;
}

int cfg_unlock(void) {
	//FIXME unlock sema
	return 0;
}