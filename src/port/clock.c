/*
 * clock.c - DOSBox Pure's time and sleeps, on the host's clock.
 *
 * DOSBox Pure asks the frontend for the time (the libretro perf interface,
 * platform.c's core_time_usec()), and its emulation thread sleeps with
 * usleep() and nanosleep() (libretro-common's retro_sleep()) while the
 * frontend shows a menu of its own. The core is compiled with those
 * renamed to the functions below (DBP_RENAMES in src/port/dosbox.mk), so
 * that its time is host_time(): on the Pi that is Circle's timer, and in
 * the PC simulator a virtual clock, which lets the tests run as fast as
 * the PC can. The time of day (CLOCK_REALTIME: DOS's clock) stays the real
 * one.
 *
 * A loop spinning on the clock until a moment comes would never end in the
 * simulator, where time only passes when something lets it: reading the
 * same time over and over (1024 times) counts as having waited 10 us, which
 * on the Pi is about what such a loop takes anyway.
 *
 * From BMAmiga. This file is part of BMDOS, a bare metal MS-DOS PC
 * emulator for the Raspberry Pi. See LICENSE for licensing details.
 */

#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "../host.h"

/* circle-newlib has clock_gettime() (the real time) but declares neither
   it nor CLOCK_MONOTONIC. */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 4
#endif
#ifdef __circle__
int clock_gettime(clockid_t clock_id, struct timespec *tp);
#endif

void bma_threads_yield(void);   /* circle/threads.c */
int bma_threads_sleep(double seconds);

double bmdos_clock_slept;   /* seconds the core has slept, for the load */

static int is_monotonic(clockid_t id)
{
	if (id == CLOCK_MONOTONIC)
		return 1;
#ifdef CLOCK_MONOTONIC_RAW
	if (id == CLOCK_MONOTONIC_RAW)
		return 1;
#endif
	return 0;
}

/* host_time(), never going backwards, as the monotonic clock. */
double bmdos_clock_now(void)
{
	static double last_host, last_given;
	static unsigned same;
	double host = host_time();
	if (host == last_host && ++same >= 1024) {
		same = 0;
		host_sleep_us(10);
		host = host_time();
	} else if (host != last_host) {
		same = 0;
	}
	last_host = host;
	if (host < last_given)
		host = last_given;
	last_given = host;
	return host;
}

int bma_clock_gettime(clockid_t id, struct timespec *ts)
{
	double t;
	if (!is_monotonic(id))
		return clock_gettime(id, ts);
	t = bmdos_clock_now();
	ts->tv_sec = (time_t) t;
	ts->tv_nsec = (long) ((t - (double) ts->tv_sec) * 1e9);
	return 0;
}

#ifdef __APPLE__
uint64_t bma_clock_gettime_nsec_np(clockid_t id)
{
	struct timespec ts;
	bma_clock_gettime(id, &ts);
	return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}
#endif

/* A sleep: BMDOS's threads are cooperative, so a thread's sleep lets the
   others run (circle/threads.c); the frontend's own thread sleeps for
   real. */
static void sleep_seconds(double s)
{
	if (bma_threads_sleep(s))
		return;
	bma_threads_yield();
	bmdos_clock_slept += s;
	host_sleep_us(s > 4000.0 ? 4000000000u : (unsigned) (s * 1e6));
}

int bma_nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (req == NULL || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return -1;
	}
	if (rem != NULL) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	sleep_seconds((double) req->tv_sec + req->tv_nsec / 1e9);
	return 0;
}

int bma_usleep(useconds_t us)
{
	sleep_seconds(us / 1e6);
	return 0;
}
