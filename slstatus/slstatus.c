/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

static int
is_bar_visible(Display *d, Window r)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;
	int visible = 1;

	Atom atom = XInternAtom(d, "_DWM_BAR_VISIBLE", True);
	if (atom == None)
		return 1;

	if (XGetWindowProperty(d, r, atom, 0, 1, False, XA_CARDINAL,
	                       &actual_type, &actual_format, &nitems, &bytes_after,
	                       &prop) == Success && prop) {
		if (nitems > 0)
			visible = *(unsigned long *)prop;
		XFree(prop);
	}
	return visible;
}

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
};

char buf[1024];
static volatile sig_atomic_t done;
static Display *dpy;

#include "config.h"

static void
terminate(const int signo)
{
	if (signo != SIGUSR1)
		done = 1;
}

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
	res->tv_nsec = a->tv_nsec - b->tv_nsec +
	               (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
	die("usage: %s [-v] [-s] [-1]", argv0);
}

int
main(int argc, char *argv[])
{
	struct sigaction act;
	struct timespec start, current, diff, intspec, wait;
	size_t i, len;
	int sflag, ret;
	char status[MAXLEN];
	const char *res;

	sflag = 0;
	ARGBEGIN {
	case 'v':
		die("slstatus-"VERSION);
		break;
	case '1':
		done = 1;
		/* FALLTHROUGH */
	case 's':
		sflag = 1;
		break;
	default:
		usage();
	} ARGEND

	if (argc)
		usage();

	memset(&act, 0, sizeof(act));
	act.sa_handler = terminate;
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_flags |= SA_RESTART;
	sigaction(SIGUSR1, &act, NULL);

	if (!sflag && !(dpy = XOpenDisplay(NULL)))
		die("XOpenDisplay: Failed to open display");

	do {
		if (!sflag && !is_bar_visible(dpy, DefaultRootWindow(dpy))) {
			struct timespec sleep_time = { .tv_sec = 2, .tv_nsec = 0 };
			nanosleep(&sleep_time, NULL);
			continue;
		}

		if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
			die("clock_gettime:");

		status[0] = '\0';
		for (i = len = 0; i < LEN(args); i++) {
			if (!(res = args[i].func(args[i].args)))
				res = unknown_str;

			if ((ret = esnprintf(status + len, sizeof(status) - len,
			                     args[i].fmt, res)) < 0)
				break;

			len += ret;
		}

		if (sflag) {
			puts(status);
			fflush(stdout);
			if (ferror(stdout))
				die("puts:");
		} else {
			if (XStoreName(dpy, DefaultRootWindow(dpy), status) < 0)
				die("XStoreName: Allocation failed");
			XFlush(dpy);
		}

		if (!done) {
			if (clock_gettime(CLOCK_MONOTONIC, &current) < 0)
				die("clock_gettime:");
			difftimespec(&diff, &current, &start);

			intspec.tv_sec = interval / 1000;
			intspec.tv_nsec = (interval % 1000) * 1E6;
			difftimespec(&wait, &intspec, &diff);

			if (wait.tv_sec >= 0 &&
			    nanosleep(&wait, NULL) < 0 &&
			    errno != EINTR)
					die("nanosleep:");
		}
	} while (!done);

	if (!sflag) {
		XStoreName(dpy, DefaultRootWindow(dpy), NULL);
		if (XCloseDisplay(dpy) < 0)
			die("XCloseDisplay: Failed to close display");
	}

	return 0;
}
