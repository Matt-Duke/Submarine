#include "redis.h"

#include <adapters/libevent.h>
#include <async.h>
#include <event.h>
#include <event2/thread.h>
#include <hiredis.h>
#include <logger.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "baseapp.h"
#include "common.h"
#include "hdlc_def.h"

// VARIABLES
redis_key_t REDIS_KEYS[REDIS_NUM_KEYS];

// FUNCTIONS

// Run once for each app
void redis_init_keys() {
    REDIS_KEYS[REDIS_ACCEL_X] =
        (redis_key_t){
            .key = "accel_x",
            .retention = 100,
            .ts = true};
    REDIS_KEYS[REDIS_ACCEL_Y] =
        (redis_key_t){
            .key = "accel_y",
            .retention = 100,
            .ts = true};
    REDIS_KEYS[REDIS_ACCEL_Z] =
        (redis_key_t){
            .key = "accel_z",
            .retention = 100,
            .ts = true};
    REDIS_KEYS[REDIS_MPC_CPU_TEMP] =
        (redis_key_t){
            .key = "mpc_temp",
            .ts = true};
    REDIS_KEYS[REDIS_MPC_UNDER_VOLT] =
        (redis_key_t){
            .key = "mpc_under_volt"};
    REDIS_KEYS[REDIS_MPC_FREQ_CAP] =
        (redis_key_t){
            .key = "mpc_freq_cap"};
    REDIS_KEYS[REDIS_MPC_THROTTLED] =
        (redis_key_t){
            .key = "mpc_throttled"};
    REDIS_KEYS[REDIS_MPC_SOFT_TEMP_LIMIT] =
        (redis_key_t){
            .key = "mpc_soft_temp_limit"};
    REDIS_KEYS[REDIS_M1_SPEED] =
        (redis_key_t){
            .key = "m1_speed",
			.retention = 1000,
            .hdlc_key = HDLC_MOTOR_SPEED,
			.ts = true,
            .hdlc_size = 2,
            .hdlc_index = 0};
    REDIS_KEYS[REDIS_M2_SPEED] =
        (redis_key_t){
            .key = "m2_speed",
			.retention = 1000,
            .hdlc_key = HDLC_MOTOR_SPEED,
			.ts = true,
            .hdlc_size = 2,
            .hdlc_index = 1};
    REDIS_KEYS[REDIS_M1_CURR] =
        (redis_key_t){
            .key = "m1_curr",
			.retention = 1000,
            .hdlc_key = HDLC_MOTOR_CURR,
			.ts = true,
            .hdlc_size = 2,
            .hdlc_index = 0};
    REDIS_KEYS[REDIS_M2_CURR] =
        (redis_key_t){
            .key = "m2_curr",
			.retention = 1000,
            .hdlc_key = HDLC_MOTOR_CURR,
			.ts = true,
            .hdlc_size = 2,
            .hdlc_index = 1};
    REDIS_KEYS[REDIS_MCU_STATE] =
        (redis_key_t){
            .key = "mcu_state",
            .hdlc_key = HDLC_STATUS,
			.ts = false,
            .hdlc_size = 4,
            .hdlc_index = 0};
    REDIS_KEYS[REDIS_HEADLIGHT] =
        (redis_key_t){
            .key = "headlight",
            .hdlc_key = HDLC_LED,
			.ts = false,
            .hdlc_size = 4,
            .hdlc_index = 0};
    REDIS_KEYS[REDIS_WATER_TEMP] =
        (redis_key_t){
            .key = "water_temp",
			.retention = 1000,
            .hdlc_key = HDLC_WATER_TEMP,
			.ts = true,
            .hdlc_size = 1,
            .hdlc_index = 0};
    REDIS_KEYS[REDIS_TOTAL_CURR] =
        (redis_key_t){
            .key = "total_curr",
			.retention = 1000,
            .hdlc_key = HDLC_TOTAL_CURR,
			.ts = true,
            .hdlc_size = 1,
            .hdlc_index = 0};
}

static void *subscriberThread(void *args) {
    struct event_base *base = (struct event_base *)args;
    event_base_dispatch(base);
}

int redis_fn_callback(void (*subCallback)(), char *topic, void *privdata) {
    if (0 != evthread_use_pthreads()) {
        LOG_ERROR("Unable to configure libevent for pthreads");
    }

    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    redisAsyncContext *c = redisAsyncConnect(REDIS_HOSTNAME, REDIS_PORT);
    if (c->err) {
        LOG_ERROR("Error: %s", c->errstr);
    }

    redisLibeventAttach(c, base);
    char cmd[100];
    if (strstr(topic, "*") != NULL) {
        sprintf(cmd, "PSUBSCRIBE %s", topic);
    } else {
        sprintf(cmd, "SUBSCRIBE %s", topic);
    }
    LOG_DEBUG("redis_fn_callback assigned to key: %s", topic);
    redisAsyncCommand(c, subCallback, privdata, cmd);

    pthread_t thread_id;
    int result = pthread_create(&thread_id, NULL, subscriberThread, (void *)base);
    return result;
}

int redis_init_context(redisContext **c) {
    struct timeval timeout = {1, 500000};

    *c = redisConnectWithTimeout(REDIS_HOSTNAME, REDIS_PORT, timeout);
    if (*c == NULL || (*c)->err) {
        if (*c) {
            LOG_ERROR("Connection error: %s", (*c)->errstr);
            redisFree(*c);
            return REDIS_CONNECTION_ERROR;
        } else {
            LOG_ERROR("Connection error: can't allocate redis context");
            redisFree(*c);
            return REDIS_ALLOCATION_FAULURE;
        }
    }
    redisReply *reply = redisCommand(*c, "PING");
    if (0 != strcmp(reply->str, "PONG")) {
        LOG_ERROR("Failed to ping Redis server. Replied: %s", reply->str);
        redisFree(*c);
        return REDIS_PING_FAILURE;
    }
    freeReplyObject(reply);
    return 0;
}

/* should be run in startup script on MPC */
int redis_create_keys(redisContext **c) {
    for (int i = 0; i < REDIS_NUM_KEYS; i++) {
        if (REDIS_KEYS[i].key == NULL) {
            break;
        }

        char cmd_s[250];
        if (REDIS_KEYS[i].ts) {
            //char label_s[250];
            //sprintf(label_s, "UNITS %s", REDIS_KEYS[i].labels.units);
            sprintf(
                cmd_s,
                "TS.CREATE %s RETENTION %d",
                REDIS_KEYS[i].key,
                REDIS_KEYS[i].retention
                //label_s
            );
        } else {
            sprintf(
                cmd_s,
                "SET %s EX %d NULL",
                REDIS_KEYS[i].key,
                REDIS_KEYS[i].retention);
        }
        LOG_DEBUG("Create Redis Key: %s", cmd_s);
        redisReply *reply = redisCommand(*c, cmd_s);
		//check errors
		freeReplyObject(reply);
    }
    return 0;
}