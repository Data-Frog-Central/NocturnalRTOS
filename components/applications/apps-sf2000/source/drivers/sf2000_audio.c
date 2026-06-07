/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/poll.h>
#include <hcuapi/snd.h>
#include <hcuapi/avsync.h>
#include <hcuapi/gpio.h>
#include <hcuapi/pinpad.h>

#include <libretro.h>

extern void frontend_log_cb(enum retro_log_level level, const char *tag, const char *fmt, ...);

static void *audio_ctx = NULL;

#define DEFAULT_SND_DEV "/dev/sndC0i2so"

#define AUDIO_BUFFER_SIZE	128 * 1024
#define AUDIO_CHANNELS		2
#define AUDIO_BITS			16

typedef struct sf2000_audio {
	bool nonblock;
	bool running;
	int snd_fd;
	struct pollfd pfd;
} sf2000_audio_t;

static pinpad_e speaker_av_pin = PINPAD_R07;

static void *sf2000_audio_init(const char *device_name, unsigned rate, float latency) {
	int ret;

	frontend_log_cb(RETRO_LOG_INFO, "AUDIO_DRIVER" ,"device=%s rate=%u\n", device_name ? device_name : "Null", rate);

	sf2000_audio_t *ctx = (sf2000_audio_t*)calloc(1, sizeof(sf2000_audio_t));
	if (!ctx)
		return NULL;

	int snd_fd = open(DEFAULT_SND_DEV, O_WRONLY);
	if (snd_fd == -1) {
		frontend_log_cb(RETRO_LOG_ERROR, "AUDIO_DRIVER" ,"open(" DEFAULT_SND_DEV ") errno=%d\n", errno);
		free(ctx);
		return NULL;
	}

	struct snd_pcm_params params = {0};
	params.access = SND_PCM_ACCESS_RW_INTERLEAVED;
	params.format = SND_PCM_FORMAT_S16_LE;
	params.sync_mode = AVSYNC_TYPE_FREERUN;
	params.align = SND_PCM_ALIGN_LEFT;
	params.rate = rate;

	//int read_size = 1536;

	params.channels = 2;
	
	uint32_t frames_in_latency = (rate * latency) / 1000;
	uint32_t frame_size = params.channels * sizeof(int16_t); // 4 bytes
	uint32_t bytes = frames_in_latency * frame_size;
	bytes = (bytes + 31) & ~31;

	params.period_size = bytes / frame_size;
	snd_pcm_uframes_t poll_size = bytes / frame_size;
	frontend_log_cb(RETRO_LOG_DEBUG, "AUDIO_DRIVER" ,"period_size=%u\n", params.period_size);
	params.periods = 4;
	params.bitdepth = 16;
	params.start_threshold = 0;
	ret = ioctl(snd_fd, SND_IOCTL_HW_PARAMS, &params);
	if (ret < 0)
		frontend_log_cb(RETRO_LOG_ERROR, "AUDIO_DRIVER" ,"SND_IOCTL_HW_PARAMS error\n");

	ret = ioctl(snd_fd, SND_IOCTL_AVAIL_MIN, &poll_size);
	if (ret < 0)
		frontend_log_cb(RETRO_LOG_ERROR, "AUDIO_DRIVER" ,"SND_IOCTL_AVAIL_MIN error\n");

	ctx->snd_fd = snd_fd;
	ctx->pfd.fd = snd_fd;
	ctx->pfd.events = POLLOUT | POLLWRNORM;

	return ctx;
}

static ssize_t sf2000_audio_write(void *data, void *buf, size_t size) {
	sf2000_audio_t* ctx = (sf2000_audio_t*)data;
	if (!ctx)
		return -1;

	if (!ctx->running)
		return -1;

	//frontend_log_cb(RETRO_LOG_DEBUG, "AUDIO_DRIVER" ,"size=%u\n", size);

	int count = 0;
	int ret;
	do {
		struct snd_xfer xfer = {0};
		xfer.data = buf;
		xfer.frames = size/4;	// 4 is 2channels*16bitsample
		//xfer.tstamp_ms = pts;
		ret = ioctl(ctx->snd_fd, SND_IOCTL_XFER, &xfer);
		if (ret < 0) {
			//frontend_log_cb(RETRO_LOG_DEBUG, "AUDIO_DRIVER" ,"poll. SND_IOCTL_XFER ret=%d\n", ret);
			poll(&ctx->pfd, 1, 100);
		}
		if (++count > 20) {
			frontend_log_cb(RETRO_LOG_ERROR, "AUDIO_DRIVER" ,"forcefully break out of the loop\n");
			break;
		}
	} while (ret < 0);

	return size;
}

static bool sf2000_audio_stop(void *data) {
	sf2000_audio_t* ctx = (sf2000_audio_t*)data;
	if (!ctx)
		return false;

	frontend_log_cb(RETRO_LOG_INFO, "AUDIO_DRIVER" ,"stop\n");

	//ioctl(ctx->snd_fd, SND_IOCTL_PAUSE, 0);
	int ret = ioctl(ctx->snd_fd, SND_IOCTL_DROP, 0);
	if (ret < 0)
		frontend_log_cb(RETRO_LOG_ERROR, "AUDIO_DRIVER", "SND_IOCTL_DROP ret=%d\n", ret);

	ctx->running = false;
	return true;
}

static bool sf2000_audio_start(void *data, bool is_shutdown) {
	sf2000_audio_t* ctx = (sf2000_audio_t*)data;
	if (!ctx)
		return false;

	frontend_log_cb(RETRO_LOG_INFO, "AUDIO_DRIVER" ,"start\n");

	//ioctl(ctx->snd_fd, SND_IOCTL_RESUME, 0);
	int ret = ioctl(ctx->snd_fd, SND_IOCTL_START, 0);
	if (ret < 0)
		frontend_log_cb(RETRO_LOG_ERROR, "AUDIO_DRIVER" ,"SND_IOCTL_START ret=%d\n", ret);

	ctx->running = true;
	return true;
}

size_t frontend_audio_batch_cb(const int16_t *data, size_t frames) {
    if (!audio_ctx)
        return 0;

    // Each frame = 2 channels, 16-bit = 4 bytes
    size_t bytes = frames * 2 * sizeof(int16_t);

    sf2000_audio_write(audio_ctx, (void *)data, bytes);
    return frames;
}

size_t frontend_mono_audio_batch_cb(const int16_t *data, size_t frames) {
    if (!audio_ctx)
        return 0;
	
	// TODO: dangerous to modify like this?
	for (size_t i=0; i < frames*2; i+=2) {
		// for single speaker output, mix to mono both channels into the first channel
		((int16_t*)data)[i] = (data[i] >> 1) + (data[i+1] >> 1);
		((int16_t*)data)[i+1] = 0;
	}

    // Each frame = 2 channels, 16-bit = 4 bytes
    size_t bytes = frames * 2 * sizeof(int16_t);

    sf2000_audio_write(audio_ctx, (void *)data, bytes);
    return frames;
}

int get_audio_occupancy(void) {
    if (!audio_ctx)
        return 50; // default to 50%

    sf2000_audio_t *ctx = (sf2000_audio_t*)audio_ctx;
    snd_pcm_uframes_t avail_frames;
    if (ioctl(ctx->snd_fd, SND_IOCTL_AVAIL_MIN, &avail_frames) < 0)
        return 50; // fallback

    size_t avail_bytes = avail_frames * AUDIO_CHANNELS * (AUDIO_BITS / 8);
    size_t used_bytes = AUDIO_BUFFER_SIZE - avail_bytes;
    int occupancy = (used_bytes * 100) / AUDIO_BUFFER_SIZE;

    if (occupancy < 0) occupancy = 0;
    if (occupancy > 100) occupancy = 100;

    return occupancy;
}

void audio_deinit(void) {
	if (!audio_ctx) return;
	sf2000_audio_t *ctx = (sf2000_audio_t*)audio_ctx;
	sf2000_audio_stop(audio_ctx);
	if (ctx->snd_fd > 0) {
		ioctl(ctx->snd_fd, SND_IOCTL_HW_FREE, 0);
    	close(ctx->snd_fd);
    	ctx->snd_fd = -1;
	}
	free(audio_ctx);
	audio_ctx = NULL;
}

void audio_init(const char *device_name, unsigned rate) {
	if (audio_ctx != NULL) audio_deinit();

	if (strcmp(device_name, "SF2000") == 0) {
		speaker_av_pin = PINPAD_R07;
	} else if (strcmp(device_name, "GB300") == 0) {
		speaker_av_pin = PINPAD_L15;
	}

	gpio_configure(speaker_av_pin, GPIO_DIR_OUTPUT); //Speaker Disable
    gpio_set_output(speaker_av_pin, false); // high = disable, low = enable;
	gpio_configure(PINPAD_T07, GPIO_DIR_OUTPUT); //Speaker fix?
    gpio_set_output(PINPAD_T07, true); // high = off, low = on;

    audio_ctx = sf2000_audio_init(device_name, rate, 34.83);
    sf2000_audio_start(audio_ctx, false);
}
