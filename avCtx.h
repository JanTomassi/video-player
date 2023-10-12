#pragma once
#include <pthread.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct
{
  int                selStream;
  AVFormatContext   *formatCtx;
  pthread_mutex_t    formatCtx_mutex;
  
  const AVCodec     *decoder;
  
  AVCodecContext    *decoderCtx;
  pthread_mutex_t    decoderCtx_mutex;
  int                is_frame;
  pthread_cond_t     frame_avail;
  
  AVPacket          *inputPkt;
  pthread_mutex_t    pkt_mutex;
  pthread_cond_t     new_package;      // Wait for a new package. Signal when a new package is present
  
  AVFrame           *inputFrame;
  pthread_mutex_t    frame_mutex;
  pthread_cond_t     decoder_send;   // Wait for a new frame to be send. Signal whan a frame is send
  pthread_cond_t     decoder_read;        // Wait for a new frame to be available. Signal when a new frame is available
  
  struct SwsContext *swsCtx_yuyv;
  pthread_mutex_t    swsCtx_mutex;

} avCtx;

__attribute_pure__ avCtx *avCtx_init (char *devname, char *vsize, char *vrate);
__attribute_pure__ void avCtx_free (avCtx **var);
int avCtx_get_one_pkt (avCtx *avctx);
int avCtx_decode_send_pkt (avCtx *avctx);
int avCtx_decode_get_frame (avCtx *avctx);
void avCtx_convert_yuvy (avCtx *avctx, AVFrame *out);
void avCtx_next_frame (avCtx *avctx);
