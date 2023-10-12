#include "avCtx.h"
#include <bits/pthreadtypes.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/cdefs.h>
#include <pthread.h>

#define DIE_AV(x) die_av (__builtin_FUNCTION (), __builtin_LINE (), x)
#define DIE(x) die (__builtin_FUNCTION (), __builtin_LINE (), x)

__attribute__ ((noreturn))
__attribute__ ((nonnull(1)))
void
die_av (const char *where, const int line, int code)
{
  fprintf (stderr, "\033[31;5mdie_av(%s:%d): %s\033[0m\n", where, line,
           av_err2str (code));
  exit (1);
}

__attribute__ ((noreturn))
__attribute__ ((nonnull(1,3)))
void
die (const char *where, const int line, char *err)
{
  fprintf (stderr, "\033[31;5mdie_av(%s:%d): %s\033[0m\n", where, line, err);
  exit (1);
}

static avCtx *
avCtx_allocate ()
{
  avCtx *new_inst = calloc (1, sizeof (avCtx));
  return new_inst;
}

__attribute_pure__ avCtx *
avCtx_init (char *devname, char *vsize, char *vrate)
{
  avdevice_register_all ();
  avCtx *avctx = avCtx_allocate ();
  int error;
  AVDictionary *inOptions = NULL;
  const AVInputFormat *inFrmt = NULL;
  
  // inFrmt = av_find_input_format ("v4l2");
  av_dict_set (&inOptions, "video_size", vsize, 0);
  av_dict_set (&inOptions, "framerate", vrate, 0);

  if ((error = avformat_open_input (&avctx->formatCtx, devname, inFrmt, &inOptions)) < 0)
    DIE_AV (error);

  if ((error = avformat_find_stream_info(avctx->formatCtx, NULL)) < 0)
    DIE_AV(error);

  av_dump_format(avctx->formatCtx, 0, devname, 0);

  for (int i = 0; i < avctx->formatCtx->nb_streams; i++)
    if (avctx->formatCtx->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      avctx->selStream = i;
      break;
    }

  if ((avctx->decoder = avcodec_find_decoder(avctx->formatCtx->streams[avctx->selStream]->codecpar->codec_id)) == 0)
    DIE("AV: Couldn't find a valid decoder for input");

  if ((avctx->decoderCtx = avcodec_alloc_context3(avctx->decoder)) == 0)
    DIE("AV: Couldn't allocate a context for the decoder");

  if ((error = avcodec_parameters_to_context(avctx->decoderCtx,
					     avctx->formatCtx->streams[avctx->selStream]->codecpar)) < 0)
    DIE_AV(error);

  if ((error = avcodec_open2(avctx->decoderCtx, avctx->decoder, NULL)) < 0)
    DIE_AV(error);

  avctx->swsCtx_yuyv
      = sws_getContext (avctx->decoderCtx->width, avctx->decoderCtx->height,
                        avctx->decoderCtx->pix_fmt, avctx->decoderCtx->width,
                        avctx->decoderCtx->height, AV_PIX_FMT_YUYV422,
                        SWS_BILINEAR, NULL, NULL, NULL);
  if (avctx->swsCtx_yuyv == NULL)
    DIE("AV: Couldn't get a sws context");

  if ((avctx->inputPkt = av_packet_alloc()) == 0)
    DIE("AV: Couldn't allocate an input packet");

  if ((avctx->inputFrame = av_frame_alloc()) == 0)
    DIE("AV: Couldn't allocate an input frame");

  if ((pthread_mutex_init(&avctx->formatCtx_mutex, NULL)) != 0)
    DIE("pthread: Couldn't inizialize the forCtx mutex");

  if ((pthread_mutex_init(&avctx->decoderCtx_mutex, NULL)) != 0)
    DIE("pthread: Couldn't inizialize the decCtx mutex");

  if ((pthread_mutex_init(&avctx->swsCtx_mutex, NULL)) != 0)
    DIE("pthread: Couldn't inizialize the sws mutex");

  if ((pthread_mutex_init(&avctx->pkt_mutex, NULL)) != 0)
    DIE("pthread: Couldn't inizialize the pkt mutex");

  if ((pthread_mutex_init(&avctx->frame_mutex, NULL)) != 0)
    DIE("pthread: Couldn't inizialize the frame mutex");

  if ((pthread_cond_init(&avctx->new_package, NULL)) != 0)
    DIE("pthread: couldn't inizialize the new package cond");

  if ((pthread_cond_init(&avctx->decoder_send, NULL)) != 0)
    DIE("pthread: couldn't inizialize the decoder send cond");

  if ((pthread_cond_init(&avctx->decoder_read, NULL)) != 0)
    DIE("pthread: couldn't inizialize the decoder read cond");

  if ((pthread_cond_init(&avctx->frame_avail, NULL)) != 0)
    DIE("pthread: couldn't inizialize the frame avail cond");
  
  return avctx;
}

void __attribute_pure__
avCtx_free (avCtx **var)
{
  avCtx *ptr = *var;
  avformat_close_input (&ptr->formatCtx);
  avcodec_send_packet (ptr->decoderCtx, NULL);
  avcodec_free_context (&ptr->decoderCtx);
  av_packet_free (&ptr->inputPkt);
  av_frame_free (&ptr->inputFrame);
  sws_freeContext (ptr->swsCtx_yuyv);

  pthread_mutex_destroy(&ptr->formatCtx_mutex);
  pthread_mutex_destroy(&ptr->decoderCtx_mutex);
  pthread_mutex_destroy(&ptr->swsCtx_mutex);
  pthread_mutex_destroy(&ptr->pkt_mutex);
  pthread_mutex_destroy(&ptr->frame_mutex);

  free (ptr);
  var = NULL;
}

int avCtx_get_one_pkt (avCtx *avctx)
{
  int error = 0;
  
  pthread_mutex_lock(&avctx->formatCtx_mutex);
  pthread_mutex_lock(&avctx->pkt_mutex);
  
  do{
    av_packet_unref (avctx->inputPkt);
    error = av_read_frame (avctx->formatCtx, avctx->inputPkt);
  }while(avctx->inputPkt->stream_index != avctx->selStream);

  pthread_mutex_unlock(&avctx->pkt_mutex);
  pthread_mutex_unlock(&avctx->formatCtx_mutex);

  if (error >= 0 || error == AVERROR_EOF)
      return error;
  else
      DIE_AV (error);
}

int
avCtx_decode_send_pkt (avCtx *avctx)
{
  int error = 0;

  pthread_mutex_lock(&avctx->decoderCtx_mutex);
  pthread_mutex_lock(&avctx->pkt_mutex);
  
  error = avcodec_send_packet (avctx->decoderCtx, avctx->inputPkt);

  pthread_cond_signal(&avctx->decoder_send);
  
  pthread_mutex_unlock(&avctx->pkt_mutex);
  pthread_mutex_unlock(&avctx->decoderCtx_mutex);

  if(error == 0 || error == AVERROR(EAGAIN) || error == AVERROR_EOF)
    return error;
  else
    DIE_AV (error);
}

int
avCtx_decode_get_frame (avCtx *avctx)
{
  int error = 0;
  
  pthread_mutex_lock(&avctx->decoderCtx_mutex);
  pthread_mutex_lock(&avctx->frame_mutex);
  
  error = avcodec_receive_frame(avctx->decoderCtx, avctx->inputFrame);
  avctx->is_frame = 1;

  pthread_cond_broadcast(&avctx->decoder_read);

  pthread_mutex_unlock(&avctx->frame_mutex);
  pthread_mutex_unlock(&avctx->decoderCtx_mutex);

  if(error == 0 || error == AVERROR(EAGAIN) || error == AVERROR_EOF)
    return error;
  else
    DIE_AV (error);
}

void
avCtx_convert_yuvy (avCtx *avctx, AVFrame *out)
{
  pthread_mutex_lock(&avctx->decoderCtx_mutex);
  if(!avctx->is_frame){
    pthread_cond_signal(&avctx->new_package);
    pthread_cond_wait(&avctx->frame_avail, &avctx->decoderCtx_mutex);
  }

  pthread_mutex_lock(&avctx->frame_mutex);

  sws_scale (avctx->swsCtx_yuyv, avctx->inputFrame->data,
	     avctx->inputFrame->linesize, 0, avctx->inputFrame->height,
             out->data, out->linesize);

  pthread_mutex_unlock(&avctx->frame_mutex);

  pthread_mutex_unlock(&avctx->decoderCtx_mutex);
}

void
avCtx_next_frame (avCtx *avctx)
{
  pthread_mutex_lock(&avctx->decoderCtx_mutex);
  pthread_cond_signal(&avctx->new_package);
  avctx->is_frame = 0;
  pthread_mutex_unlock(&avctx->decoderCtx_mutex);
}
