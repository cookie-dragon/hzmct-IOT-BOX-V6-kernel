/* Industrialio buffer test code.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is primarily intended as an example application.
 * Reads the current buffer setup from sysfs and starts a short capture
 * from the specified device, pretty printing the result after appropriate
 * conversion.
 *
 * Command line parameters
 * generic_buffer -n <device_name> -t <trigger_name>
 * If trigger name is not specified the program assumes you want a dataready
 * trigger associated with the device and goes looking for it.
 *
 */

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <linux/types.h>
#include <string.h>
#include <poll.h>
#include "iio_utils.h"

/**
 * size_from_channelarray() - calculate the storage size of a scan
 * @channels: the channel info array
 * @num_channels: size of the channel info array
 *
 * Has the side effect of filling the channels[i].location values used
 * in processing the buffer output.
 **/
int size_from_channelarray(struct iio_channel_info *channels, int num_channels)
{
	int bytes = 0;
	int i = 0;
	while (i < num_channels) {
		if (bytes % channels[i].bytes == 0)
			channels[i].location = bytes;
		else
			channels[i].location = bytes - bytes%channels[i].bytes
				+ channels[i].bytes;
		bytes = channels[i].location + channels[i].bytes;
		i++;
	}
	return bytes;
}

void print2byte(int input, struct iio_channel_info *info)
{
	/* shift before conversion to avoid sign extension
	   of left aligned data */
	input = input >> info->shift;
	if (info->is_signed) {
		int16_t val = input;
		val &= (1 << info->bits_used) - 1;
		val = (int16_t)(val << (16 - info->bits_used)) >>
			(16 - info->bits_used);
		printf("%05f  ", val,
		       (float)(val + info->offset)*info->scale);
	} else {
		uint16_t val = input;
		val &= (1 << info->bits_used) - 1;
		printf("%05f ", ((float)val + info->offset)*info->scale);
	}
}
/**
 * process_scan() - print out the values in SI units
 * @data:		pointer to the start of the scan
 * @infoarray:		information about the channels. Note
 *  size_from_channelarray must have been called first to fill the
 *  location offsets.
 * @num_channels:	the number of active channels
 **/
void process_scan(char *data,
		  struct iio_channel_info *infoarray,
		  int num_channels)
{
	int k;
	for (k = 0; k < num_channels; k++)
		switch (infoarray[k].bytes) {
			/* only a few cases implemented so far */
		case 2:
			print2byte(*(uint16_t *)(data + infoarray[k].location),
				   &infoarray[k]);
			break;
		case 8:
			if (infoarray[k].is_signed) {
				int64_t val = *(int64_t *)
					(data +
					 infoarray[k].location);
				if ((val >> infoarray[k].bits_used) & 1)
					val = (val & infoarray[k].mask) |
						~infoarray[k].mask;
				/* special case for timestamp */
				if (infoarray[k].scale == 1.0f &&
				    infoarray[k].offset == 0.0f)
					printf(" %lld", val);
				else
					printf("%05f ", ((float)val +
							 infoarray[k].offset)*
					       infoarray[k].scale);
			}
			break;
		default:
			break;
		}
	printf("\n");
}

int main(int argc, char **argv)
{
	unsigned long num_loops = 2;
	unsigned long timedelay = 1000000;
	unsigned long buf_len = 128;


	int ret, c, i, j, toread;

	FILE *fp_ev;
	int fp;

	int num_channels;
	char *device_name = NULL;
	char *dev_dir_name, *buf_dir_name;

	char *data;
	ssize_t read_size;
	int dev_num;
	char *buffer_access;
	int scan_size;
	int noevents = 0;
	char *dummy;

	struct iio_channel_info *infoarray;

	while ((c = getopt(argc, argv, "l:w:c:et:n:")) != -1) {
		switch (c) {
		case 'n':
			device_name = optarg;
			break;
		case 'e':
			noevents = 1;
			break;
		case 'c':
			num_loops = strtoul(optarg, &dummy, 10);
			break;
		case 'w':
			timedelay = strtoul(optarg, &dummy, 10);
			break;
		case 'l':
			buf_len = strtoul(optarg, &dummy, 10);
			break;
		case '?':
			return -1;
		}
	}

	if (device_name == NULL)
		return -1;

	/* Find the device requested */
	dev_num = find_type_by_name(device_name, "iio:device");
	if (dev_num < 0) {
		printf("Failed to find the %s\n", device_name);
		ret = -ENODEV;
		goto error_ret;
	}
	printf("iio device number being used is %d\n", dev_num);

	asprintf(&dev_dir_name, "%siio:device%d", iio_dir, dev_num);

	/*
	 * Parse the files in scan_elements to identify what channels are
	 * present
	 */
	ret = build_channel_array(dev_dir_name, &infoarray, &num_channels);
	if (ret) {
		printf("Problem reading scan element information\n");
		printf("diag %s\n", dev_dir_name);
		goto error_ret;
	}

	/*
	 * Construct the directory name for the associated buffer.
	 * As we know that the lis3l02dq has only one buffer this may
	 * be built rather than found.
	 */
	ret = asprintf(&buf_dir_name,
		       "%siio:device%d/buffer", iio_dir, dev_num);
	if (ret < 0) {
		ret = -ENOMEM;
		goto error_ret;
	}
	printf("%s \n", dev_dir_name);
	/* Setup ring buffer parameters */
	ret = write_sysfs_int("length", buf_dir_name, buf_len);
	if (ret < 0)
		goto error_free_buf_dir_name;
	/* Enable the buffer */
	ret = write_sysfs_int("enable", buf_dir_name, 1);
	if (ret < 0)
		goto error_free_buf_dir_name;

	ret = asprintf(&buffer_access, "/dev/iio:device%d", dev_num);
	if (ret < 0) {
		ret = -ENOMEM;
		goto error_free_buf_dir_name;
	}

	/* Attempt to open non blocking the access dev */
	fp = open(buffer_access, O_RDONLY | O_NONBLOCK);
	if (fp == -1) { /*If it isn't there make the node */
		printf("Failed to open %s\n", buffer_access);
		ret = -errno;
		goto error_free_buf_dir_name;
	}

	scan_size = size_from_channelarray(infoarray, num_channels);
	data = malloc(scan_size*buf_len);
	if (!data) {
		ret = -ENOMEM;
		goto error_close_buffer_access;
	}

	/* Wait for events 10 times */
	for (j = 0; j < num_loops; j++) {
		usleep(timedelay);
		read_size = read(fp,
				 data,
				 buf_len*scan_size);
		if (read_size == -EAGAIN) {
			printf("nothing available\n");
			continue;
		}
		for (i = 0; i < read_size/4; i++, data+=4)
			printf("ADC Value: %d\n", *(long*)data);
	}

	/* Stop the ring buffer */
	ret = write_sysfs_int("enable", buf_dir_name, 0);
	if (ret < 0)
		goto error_close_buffer_access;

error_close_buffer_access:
	close(fp);
error_free_buf_dir_name:
	free(buf_dir_name);
error_ret:
	return ret;
}
