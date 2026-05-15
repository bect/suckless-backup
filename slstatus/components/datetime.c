/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <time.h>

#include "../slstatus.h"
#include "../util.h"

const char *
datetime(const char *fmt)
{
	time_t t;
	struct tm *tm_info;
	int day;
	const char *suffix = "th";
	char expanded_fmt[512];
	size_t i = 0, j = 0;

	t = time(NULL);
	tm_info = localtime(&t);
	day = tm_info->tm_mday;

	/* Determine mathematical English ordinal suffix (st, nd, rd, th) */
	if (day < 11 || day > 13) {
		switch (day % 10) {
			case 1:  suffix = "st"; break;
			case 2:  suffix = "nd"; break;
			case 3:  suffix = "rd"; break;
			default: suffix = "th"; break;
		}
	}

	/* Replace custom %o token with computed suffix to preserve generic strftime */
	while (fmt[i] && j < sizeof(expanded_fmt) - 5) {
		if (fmt[i] == '%' && fmt[i + 1] == 'o') {
			/* Inject suffix */
			const char *s = suffix;
			while (*s) {
				expanded_fmt[j++] = *s++;
			}
			i += 2;
		} else {
			expanded_fmt[j++] = fmt[i++];
		}
	}
	expanded_fmt[j] = '\0';

	if (!strftime(buf, sizeof(buf), expanded_fmt, tm_info)) {
		warn("strftime: Result string exceeds buffer size");
		return NULL;
	}

	return buf;
}
