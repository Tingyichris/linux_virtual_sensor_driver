#ifndef SP_VIRT_SENSOR_UAPI_H
#define SP_VIRT_SENSOR_UAPI_H

#include <linux/ioctl.h>
#include <stdint.h>

#define SP_SENSOR_IOCTL_MAGIC 's'

struct sp_sensor_sample {
	uint32_t seq;
	int32_t temperature_milli_c;
	int32_t light_lux;
};

#define SP_SENSOR_GET_COUNT _IOR(SP_SENSOR_IOCTL_MAGIC, 1, int)

#endif

