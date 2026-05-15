/* See LICENSE file for copyright and license details. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
	#include <stdint.h>

	#define NET_RX_BYTES "/sys/class/net/%s/statistics/rx_bytes"
	#define NET_TX_BYTES "/sys/class/net/%s/statistics/tx_bytes"

	const char *
	netspeed_rx(const char *interface)
	{
		uintmax_t oldrxbytes;
		static uintmax_t rxbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];

		oldrxbytes = rxbytes;

		if (esnprintf(path, sizeof(path), NET_RX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &rxbytes) != 1)
			return NULL;
		if (oldrxbytes == 0)
			return NULL;

		return fmt_human((rxbytes - oldrxbytes) * 1000 / interval,
		                 1024);
	}

	const char *
	netspeed_tx(const char *interface)
	{
		uintmax_t oldtxbytes;
		static uintmax_t txbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];

		oldtxbytes = txbytes;

		if (esnprintf(path, sizeof(path), NET_TX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &txbytes) != 1)
			return NULL;
		if (oldtxbytes == 0)
			return NULL;

		return fmt_human((txbytes - oldtxbytes) * 1000 / interval,
		                 1024);
	}

	static const char *
	get_default_interface(void)
	{
		static char iface[32];
		FILE *fp;
		char line[256];
		char dev[32], dest[32];

		if (!(fp = fopen("/proc/net/route", "r")))
			return NULL;

		/* Skip route header */
		if (!fgets(line, sizeof(line), fp)) {
			fclose(fp);
			return NULL;
		}

		while (fgets(line, sizeof(line), fp)) {
			if (sscanf(line, "%31s %31s", dev, dest) == 2) {
				if (strcmp(dest, "00000000") == 0) {
					strcpy(iface, dev);
					fclose(fp);
					return iface;
				}
			}
		}

		fclose(fp);
		return NULL;
	}

	const char *
	netspeed(const char *interface)
	{
		uintmax_t oldrxbytes, oldtxbytes;
		static uintmax_t rxbytes, txbytes;
		static char last_interface[32] = "";
		extern const unsigned int interval;
		char path_rx[PATH_MAX], path_tx[PATH_MAX];
		char rx_human[64], tx_human[64];
		const char *fmt;

		/* Automatically scan routing engine if auto/NULL is provided */
		if (!interface || strcmp(interface, "auto") == 0) {
			interface = get_default_interface();
			if (!interface)
				return "";
		}

		/* If interface has hot-swapped, reset trackers to prevent massive mathematical spikes! */
		if (strcmp(interface, last_interface) != 0) {
			rxbytes = 0;
			txbytes = 0;
			strncpy(last_interface, interface, sizeof(last_interface) - 1);
		}

		oldrxbytes = rxbytes;
		oldtxbytes = txbytes;

		if (esnprintf(path_rx, sizeof(path_rx), NET_RX_BYTES, interface) < 0 ||
		    esnprintf(path_tx, sizeof(path_tx), NET_TX_BYTES, interface) < 0)
			return "";

		if (pscanf(path_rx, "%ju", &rxbytes) != 1 ||
		    pscanf(path_tx, "%ju", &txbytes) != 1)
			return ""; /* Interface missing/offline -> hide completely! */

		if (oldrxbytes == 0 || oldtxbytes == 0)
			return ""; /* First loop delta initialization */

		/* Safely duplicate human formatted string from static global buffer */
		fmt = fmt_human((rxbytes - oldrxbytes) * 1000 / interval, 1024);
		if (!fmt) return "";
		strcpy(rx_human, fmt);

		/* Safely duplicate human formatted string from static global buffer */
		fmt = fmt_human((txbytes - oldtxbytes) * 1000 / interval, 1024);
		if (!fmt) return "";
		strcpy(tx_human, fmt);

		/* Atomic return combining vector icons, dynamically formatted scales, and separators */
		return bprintf(" %sB/s  %sB/s | ", rx_human, tx_human);
	}
#elif defined(__OpenBSD__) | defined(__FreeBSD__)
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/socket.h>

	const char *
	netspeed_rx(const char *interface)
	{
		struct ifaddrs *ifal, *ifa;
		struct if_data *ifd;
		uintmax_t oldrxbytes;
		static uintmax_t rxbytes;
		extern const unsigned int interval;
		int if_ok = 0;

		oldrxbytes = rxbytes;

		if (getifaddrs(&ifal) < 0) {
			warn("getifaddrs failed");
			return NULL;
		}
		rxbytes = 0;
		for (ifa = ifal; ifa; ifa = ifa->ifa_next)
			if (!strcmp(ifa->ifa_name, interface) &&
			   (ifd = (struct if_data *)ifa->ifa_data))
				rxbytes += ifd->ifi_ibytes, if_ok = 1;

		freeifaddrs(ifal);
		if (!if_ok) {
			warn("reading 'if_data' failed");
			return NULL;
		}
		if (oldrxbytes == 0)
			return NULL;

		return fmt_human((rxbytes - oldrxbytes) * 1000 / interval,
		                 1024);
	}

	const char *
	netspeed_tx(const char *interface)
	{
		struct ifaddrs *ifal, *ifa;
		struct if_data *ifd;
		uintmax_t oldtxbytes;
		static uintmax_t txbytes;
		extern const unsigned int interval;
		int if_ok = 0;

		oldtxbytes = txbytes;

		if (getifaddrs(&ifal) < 0) {
			warn("getifaddrs failed");
			return NULL;
		}
		txbytes = 0;
		for (ifa = ifal; ifa; ifa = ifa->ifa_next)
			if (!strcmp(ifa->ifa_name, interface) &&
			   (ifd = (struct if_data *)ifa->ifa_data))
				txbytes += ifd->ifi_obytes, if_ok = 1;

		freeifaddrs(ifal);
		if (!if_ok) {
			warn("reading 'if_data' failed");
			return NULL;
		}
		if (oldtxbytes == 0)
			return NULL;

		return fmt_human((txbytes - oldtxbytes) * 1000 / interval,
		                 1024);
	}
#endif
