#ifndef _HCUAPI_RESAMPLE_H_
#define _HCUAPI_RESAMPLE_H_

#include <kernel/drivers/snd.h>
#include <errno.h>

#define MAX_RESCALE_CHANNELS 8

struct rescale_hermite {
	unsigned int delta;
	unsigned int phase;
	int32_t  history[MAX_RESCALE_CHANNELS][3];

	unsigned int frequency;
	unsigned int frequency_out;
};

typedef struct {
  int S_16_32[8];
  int S_32_24[8];
  int S_24_48[8];
} WebRtcSpl_State16khzTo48khz;

typedef struct {
  int S_48_48[16];
  int S_48_32[8];
  int S_32_16[8];
} WebRtcSpl_State48khzTo16khz;
struct rescale_webrtc {
	int sample_rate_in;
	int sample_rate_out;
	int16_t *tmp_mem;
	size_t tmp_mem_size;
	void *tmp_data_planner;
	size_t tmp_data_planner_size;
	int16_t remains_buf[2][480*2];
	size_t remains[2];
	int tmp_value[496];
	int state[10][8];
	WebRtcSpl_State16khzTo48khz state2[2];
	WebRtcSpl_State48khzTo16khz state3[2];
	int access;
	int format;
	int align;
};
void init_hermite_scale(struct rescale_hermite *r, int in_freq, int out_freq);
void resample_flush(struct rescale_hermite *r);
int resample_process(struct rescale_hermite *r, int *in, int *out, snd_pcm_uframes_t frames,
	     snd_pcm_uframes_t ipitch, snd_pcm_access_t access,
	     snd_pcm_format_t format, uint8_t channels_in);

int resample_process_multi_times(struct rescale_webrtc *param,void *in,void *out,
         int frames,int ipitch,uint8_t channels_in);
void dsp_eq_init(int rate, int channels, int bitdepth);
void dsp_eq_process(void *data, uint32_t size);
void dsp_eq_enable(int enable);
void dsp_set_eq_coefs(int band, int cutoff, int q, int gain);

#endif	/* _HCUAPI_RESAMPLE_H_ */
