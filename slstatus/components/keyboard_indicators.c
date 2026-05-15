/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#include "../slstatus.h"
#include "../util.h"

/*
 * fmt consists of uppercase or lowercase 'c' for caps lock and/or 'n' for num
 * lock, each optionally followed by '?', in the order of indicators desired.
 * If followed by '?', the letter with case preserved is included in the output
 * if the corresponding indicator is on.  Otherwise, the letter is always
 * included, lowercase when off and uppercase when on.
 */
const char *
keyboard_indicators(const char *unused)
{
	Display *dpy;
	XKeyboardState state;
	int caps_on, num_on;

	if (!(dpy = XOpenDisplay(NULL))) {
		warn("XOpenDisplay: Failed to open display");
		return NULL;
	}
	XGetKeyboardControl(dpy, &state);
	XCloseDisplay(dpy);

	/* Extract Caps Lock (bit 0) and Num Lock (bit 1) states */
	caps_on = state.led_mask & 1;
	num_on = state.led_mask & 2;

	/* Format output: Only show icons and separator when they are actively ON */
	if (caps_on && num_on)
		return bprintf("  | ");
	else if (caps_on)
		return bprintf(" | ");
	else if (num_on)
		return bprintf(" | ");

	return ""; /* Both are OFF, show nothing to preserve screen space */
}
