#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <hiredis-vip/hiredis.h>
#include <logger/logger.h>

#include <common.h>
#include <redis_def.h>
#include <baseapp.h>

//#include "light.h"
//#include "imu.h"

extern const char *__progname;
redisContext *c;
redisReply *reply;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

smAppClass_t state_machine;
//ImuClass_t imu;

void transition_to_init_state(smAppClass_t *app) {
	if (app->curr_state == APP_STATE_INIT) {

		pthread_t thread_id;
		pthread_create(&thread_id, NULL, heartbeatThread, (void*) &state_machine);
		
		mutex = init_redis(&c, REDIS_HOSTNAME, REDIS_PORT);

		//imuClassInit(&imu);
	}
}

void do_running_state(smAppClass_t *app) {
	
}

int main(int argc, char *argv[])
{
	init_logging();
	//app_transition_table[APP_STATE_INIT][APP_STATE_INIT] = transition_to_init_state;
	app_run_table[APP_STATE_RUNNING] = do_running_state;

	appInit(&state_machine);

	while (1) {
		state_machine.run(&state_machine);

	}
}