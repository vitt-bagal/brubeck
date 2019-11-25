#ifndef __BRUBECK__H_
#define __BRUBECK__H_

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define _GNU_SOURCE
#include <math.h>
#undef _GNU_SOURCE

#define BRUBECK_STATS 1
#define MAX_ADDR 256

typedef double value_t;
typedef uint64_t hash_t;

struct brubeck_server;
struct brubeck_metric;

#include "backend.h"
#include "histogram.h"
#include "ht.h"
#include "jansson.h"
#include "log.h"
#include "metric.h"
#include "sampler.h"
#include "server.h"
#include "slab.h"
#include "utils.h"

#endif
