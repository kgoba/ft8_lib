
#ifndef _INCLUDE_FT8_H_
#define _INCLUDE_FT8_H_

// supported protocols
typedef enum { PROTO_FT4, PROTO_FT8 } ftx_protocol_t;

// decoder callback
typedef void (*ftx_decode_callback_t)(char *message, float frequency, float time_dev, float snr, int score, void *ctx);

// decode FT4 or FT8 signal
int ftx_decode(float *signal, int num_samples, int sample_rate, ftx_protocol_t protocol, ftx_decode_callback_t callback, void *ctx);

// generate FT4 or FT8 signal for message
int ftx_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate, ftx_protocol_t protocol);

#endif // _INCLUDE_FT8_H_
