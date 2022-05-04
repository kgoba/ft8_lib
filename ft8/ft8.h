
#ifndef _INCLUDE_FT8_H_
#define _INCLUDE_FT8_H_

// supported protocols
typedef enum { PROTO_FT4, PROTO_FT8 } ftx_protocol_t;

// decoder callback
typedef void (*ft8_decode_callback_t)(char *message, float frequency, float time_dev, float snr, int score, void *ctx);

// decode FT8 signal, call callback for every decoded message
int ftx_decode(float *signal, int num_samples, int sample_rate, ft8_decode_callback_t callback, void *ctx);

// generate FT4 signal for message
int ft4_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate);

// generate FT8 signal for message
int ft8_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate);

#endif // _INCLUDE_FT8_H_
