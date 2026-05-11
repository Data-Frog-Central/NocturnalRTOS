#ifndef SF2000_AUDIO_H__
#define SF2000_AUDIO_H__

int get_audio_occupancy(void);
void audio_init(const char *device_name, unsigned rate);
size_t frontend_audio_batch_cb(const int16_t *data, size_t frames);
size_t frontend_mono_audio_batch_cb(const int16_t *data, size_t frames);

#endif // SF2000_AUDIO_H__