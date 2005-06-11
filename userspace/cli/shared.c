/*
 *    This file is part of LiSA Command Line Interface.
 *
 *    LiSA Command Line Interface is free software; you can redistribute it 
 *    and/or modify it under the terms of the GNU General Public License 
 *    as published by the Free Software Foundation; either version 2 of the 
 *    License, or (at your option) any later version.
 *
 *    LiSA Command Line Interface is distributed in the hope that it will be 
 *    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LiSA Command Line Interface; if not, write to the Free 
 *    Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *    MA  02111-1307  USA
 */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "shared.h"

extern int errno;

char *key_file_path = "/tmp/cli";
struct cli_config *cfg = NULL;
int shmid;
int semid;
static int cfg_locked = 0;

static struct sembuf P = {
	.sem_num = 0,
	.sem_op = -1,
	.sem_flg = 0,
};

static struct sembuf V = {
	.sem_num = 0,
	.sem_op = 1,
	.sem_flg = 0,
};

void cfg_init_data(void) {
	memset(cfg, 0, sizeof(struct cli_config));
}

int cfg_init(void) {
	key_t key;
	int fd;
	int init_data = 0;

	if (cfg) return 0;
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
		/* FIXME try to create exclusively, and if it fails then try
		   again without creating; this is to avoid races if someone else
		   creates it in the meantime */
		if(shmid == -1)
			return -3;
	}
	cfg = shmat(shmid, NULL, 0);
	if((int)cfg == -1)
		return -4;
	if(init_data)
		cfg_init_data();
	if((semid = semget(key, 1, 0600)) == -1) {
		semid = semget(key, 1, IPC_CREAT | 0600);
		/* FIXME try to create exclusively, and if it fails then try
		   again without creating; this is to avoid races if someone else
		   creates it in the meantime */
		if(semid == -1)
			return -5;
		/* "Initialize" the semaphore to 1 */
		semop(semid, &V, 1);
	}
	return 0;
}

int cfg_lock(void) {
	if(!(cfg_locked++))
		semop(semid, &P, 1);
	return 0;
}

int cfg_unlock(void) {
	if(!(--cfg_locked))
		semop(semid, &V, 1);
	return 0;
}

static void sw_redisplay_password(void) {
	fprintf(rl_outstream, "\rPassword: ");
	fflush(rl_outstream);
}

int cfg_checkpass(int retries, int (*validator)(char *, void *), void *arg) {
	rl_voidfunc_t *old_redisplay = rl_redisplay_function;
	char *pw;
	int i;

	rl_redisplay_function = sw_redisplay_password;
	for(i = 0; i < retries; i++) {
		pw = readline(NULL);
		if(validator(pw, arg))
			break;
	}
	rl_redisplay_function = old_redisplay;
	return i < retries;
}

static void sw_redisplay_void(void) {
}

int cfg_waitcr(void) {
	rl_voidfunc_t *old_redisplay = rl_redisplay_function;

	rl_redisplay_function = sw_redisplay_void;
	readline(NULL);
	rl_redisplay_function = old_redisplay;
	return 0;
}

int read_key() {
	int ret;
	struct termios t_old, t_new;

	tcgetattr(0, &t_old);
	t_new = t_old;
	t_new.c_lflag = ~ICANON;
	t_new.c_cc[VTIME] = 0;
	t_new.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &t_new);
	ret = getchar();
	tcsetattr(0, TCSANOW, &t_old);
	return ret;
}
