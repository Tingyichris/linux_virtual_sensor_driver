#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "sp_virt_sensor_uapi.h"

#define DEVICE_PATH "/dev/sp_virt_sensor"

int main(void)
{
	struct sp_sensor_sample tx = {
		.seq = 1,
		.temperature_milli_c = 26375,
		.light_lux = 420,
	};
	struct sp_sensor_sample rx;
	struct pollfd pfd;
	int fd;
	int count = -1;

	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", DEVICE_PATH, strerror(errno));
		return 1;
	}

	if (write(fd, &tx, sizeof(tx)) != sizeof(tx)) {
		fprintf(stderr, "write failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	if (ioctl(fd, SP_SENSOR_GET_COUNT, &count) < 0) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	printf("buffer count after write: %d\n", count);

	pfd.fd = fd;
	pfd.events = POLLIN;
	if (poll(&pfd, 1, 1000) <= 0) {
		fprintf(stderr, "poll timeout or failed\n");
		close(fd);
		return 1;
	}

	if (read(fd, &rx, sizeof(rx)) != sizeof(rx)) {
		fprintf(stderr, "read failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("sample seq=%u temp=%d.%03d C light=%d lux\n",
	       rx.seq,
	       rx.temperature_milli_c / 1000,
	       rx.temperature_milli_c % 1000,
	       rx.light_lux);

	close(fd);
	return 0;
}

