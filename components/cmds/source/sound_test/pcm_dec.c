#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hcuapi/common.h>
#include <hcuapi/kshm.h>
#include <hcuapi/auddec.h>
#include <hcuapi/codec_id.h>
#include <sys/ioctl.h>
#include <hcuapi/snd.h>
//#include "pcm_dec.h"

#define ALOGI printf
#define ALOGE printf
#define ALOGD printf

static uint8_t g_volume = 100;

struct pcm_decoder {
	struct audio_config cfg;
	int fd;
};

void *pcm_decoder_init(int bits, int channels, int samplerate)
{
	struct pcm_decoder *p = (struct pcm_decoder *)malloc(sizeof(struct pcm_decoder));
	memset(&p->cfg, 0, sizeof(struct audio_config));
	p->cfg.bits_per_coded_sample = bits;
	p->cfg.channels = channels;
	p->cfg.sample_rate = samplerate;
	p->cfg.codec_id = HC_AVCODEC_ID_PCM_S16LE;

	p->fd = open("/dev/auddec", O_RDWR);
	printf("p->fd = %x\n", p->fd);
	if (p->fd < 0) {
		printf("Open /dev/auddec error.");
		free(p);
		return NULL;
	}

	if (ioctl(p->fd, AUDDEC_INIT, &p->cfg) != 0) {
		printf("Init auddec error.");
		close(p->fd);
		free(p);
		return NULL;
	}
	printf("AUDDEC_START\n");
	ioctl(p->fd, AUDDEC_START, 0);
	return p;
}

int pcm_decode(void *phandle, uint8_t *audio_frame, size_t packet_size)
{
	struct pcm_decoder *p = (struct pcm_decoder *)phandle;
	AvPktHd pkthd = { 0 };

	pkthd.dur = 0;
	pkthd.size = packet_size;
	pkthd.flag = AV_PACKET_ES_DATA;
	pkthd.pts = -1;

	while (1) {
		if (write(p->fd, (uint8_t *)&pkthd, sizeof(AvPktHd)) !=
		    sizeof(AvPktHd)) {
			usleep(20 * 1000);
			continue;
		}
		break;
	}
	while (1) {
		if (write(p->fd, audio_frame, packet_size) != (int)packet_size) {
			usleep(20 * 1000);
			continue;
		}
		break;
	}

	return 0;
}

void pcm_decoder_flush(void *phandle)
{
	struct pcm_decoder *p = (struct pcm_decoder *)phandle;
	ioctl(p->fd, AUDDEC_FLUSH, 0);
}

void pcm_decoder_destroy(void *phandle)
{
	struct pcm_decoder *p = (struct pcm_decoder *)phandle;
	if (!p)
		return;

	if (p->fd > 0) {
		close(p->fd);
	}
	free(p);
}

void pcm_decode_eos(void *phandle)
{
	struct pcm_decoder *p = (struct pcm_decoder *)phandle;
	AvPktHd pkthd = { 0 };
	pkthd.flag = AV_PACKET_EOS;
	while (1) {
		if (write(p->fd, (uint8_t *)&pkthd, sizeof(AvPktHd)) !=
		    sizeof(AvPktHd)) {
			usleep(20 * 1000);
			//printf("write1\n");
			continue;
		}
		break;
	}

	while (1) {
		int eos;
		ioctl(p->fd, AUDDEC_CHECK_EOS, &eos);
		if (!eos)
			usleep(50 * 1000);
		else
			break;
	}

	return;
}

int pcm_play_task(char *file_path, int bits, int channels, int samplerate)
{
	int process_units, ret;
	void *hdl = NULL;
	int buf_size = 4096;
	uint8_t *data = NULL;
	int pcm_file = -1;
	char *pcm_url = file_path;

	pcm_file = open(pcm_url, O_RDONLY);
	printf("pcm_file is %d\n", pcm_file);
	if (pcm_file <= 0) {
		printf("can not open pcm url: %s\n", pcm_url);
		return -1;
	}
	hdl = pcm_decoder_init(bits, channels, samplerate);
	printf("hdl = %x\n", (int)hdl);

	data = malloc(buf_size);
	if (data == NULL) {
		close(pcm_file);
		return -1;
	}

	while (1) {
		ret = read(pcm_file, data, buf_size);
		//printf("read size %d\n", (int)ret);
		if (ret != buf_size) {
			break;
		}

		ret = pcm_decode(hdl, data, buf_size);
		if (ret) {
			printf("pcm_decode error\n");
			break;
		}
	}

	pcm_decode_eos(hdl);

	pcm_decoder_destroy(hdl);
	close(pcm_file);
	free(data);

	return 0;
}

static int set_i2so_volume(uint8_t volume)
{
	int snd_fd = -1;

	snd_fd = open("/dev/sndC0i2so", O_WRONLY);
	if (snd_fd < 0) {
		printf ("open snd_fd %d failed\n", snd_fd);
		return -1;
	}

	ioctl(snd_fd, SND_IOCTL_SET_VOLUME, &volume);
	volume = 0;
	ioctl(snd_fd, SND_IOCTL_GET_VOLUME, &g_volume);
	printf("current volume is %d\n", g_volume);

	close(snd_fd);
	return 0;
}

int pcm_dec(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	int bits = 0;
	int channels = 0;
	int samplerate = 0;
	char *uri = NULL;
	int opt;

    opterr = 0;
    optind = 0;
	printf("cmd sample: pcm_play patch bits channels samplerate\n");
	printf("cmd sample: pcm_play /media/sda/test.wav -b 16 -c 1 -s 16000\n");

	set_i2so_volume(100);

	uri = argv[1];
	if (!uri) {
		printf("need path! \n");
		return -1;
	}
	while((opt = getopt(argc , argv , "b:c:s:")) != EOF)
    {
        switch(opt)
        {
            case 'b':
                bits = atoi(optarg);
				printf("bits is %d\n", bits);
				if (!bits || bits == 1 || bits == 2) {
					printf("wrong bits! for example 8/16/24/32, default set to 16 bits \n");
					bits = 16;
				}
                break;
            case 'c':
                channels = atoi(optarg);
				printf("channels is %d\n", channels);
				if (channels != 1 && channels != 2) {
					printf("need channel number, 1 or 2,  default set to 2 \n");
					channels = 2;
				}
                break;
            case 's':
                samplerate = atoi(optarg);
				printf("samplerate is %d\n", samplerate);
				if (samplerate < 8000) {
					printf("need write samplerate! such as 11025/16000/44100/48000,  default set to 44100Hz \n");
					samplerate = 44100;
				}
                break;
            default:
				printf("no this param: %d, please check!\n", opt);
                break;
        }
    }

	pcm_play_task(uri, bits, channels, samplerate);
	return 0;
}