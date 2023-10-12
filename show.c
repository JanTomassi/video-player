#include "show.h"
#include "avCtx.h"
#include "sdlCtx.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <bits/types/struct_sched_param.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
  bool running;
  SDL_Event event;
  pthread_mutex_t run_mutex;
} showState;

typedef struct
{
  avCtx *avctx;
  showState *state;
} thread_arg;

static void
showState_SDL_Event (showState *state)
{
  SDL_PollEvent(&state->event);
  switch(state->event.type)
    {
    case SDL_QUIT:
      pthread_mutex_lock(&state->run_mutex);
      state->running = false;
      pthread_mutex_unlock(&state->run_mutex);
      break;
    }
}

void *
show_sender (void *arg)
{
  thread_arg *th_arg = arg;
  avCtx *avctx = th_arg->avctx;
  showState *state = th_arg->state;
  int get_res = 0;
  int send_res = 0;

  while(1){
    pthread_mutex_lock(&state->run_mutex);
    if (!state->running) {
      pthread_mutex_unlock(&state->run_mutex);
      break; // Exit the thread
    }
    pthread_mutex_unlock(&state->run_mutex);


    if(send_res == 0)
      get_res = avCtx_get_one_pkt(avctx);
    send_res = avCtx_decode_send_pkt (avctx);

    if(send_res == AVERROR(EAGAIN)){      
      pthread_mutex_lock(&avctx->decoderCtx_mutex);
      pthread_cond_wait(&avctx->decoder_read, &avctx->decoderCtx_mutex);
      pthread_mutex_unlock(&avctx->decoderCtx_mutex);
    }
    if(send_res == AVERROR_EOF || get_res == AVERROR_EOF){
      pthread_mutex_lock(&state->run_mutex);
      state->running = false;
      pthread_mutex_unlock(&state->run_mutex);
    }
  }
  return NULL;
}

void *
show_reciver (void *arg)
{
  thread_arg *th_arg = arg;
  avCtx *avctx = th_arg->avctx;
  showState *state = th_arg->state;
  int send_res = 0;

  while(1){
    pthread_mutex_lock(&state->run_mutex);
    if (!state->running) {
      pthread_mutex_unlock(&state->run_mutex);
      break; // Exit the thread
    }
    pthread_mutex_unlock(&state->run_mutex);


    send_res = avCtx_decode_get_frame(avctx);
    
    if(send_res == AVERROR(EAGAIN)){

      pthread_mutex_lock(&avctx->decoderCtx_mutex);

      pthread_cond_wait(&avctx->decoder_send, &avctx->decoderCtx_mutex);

      pthread_mutex_unlock(&avctx->decoderCtx_mutex);
    }else if(send_res == 0){

      pthread_cond_signal(&avctx->frame_avail);

      pthread_mutex_lock(&avctx->frame_mutex);

      pthread_cond_wait(&avctx->new_package, &avctx->frame_mutex);

      pthread_mutex_unlock(&avctx->frame_mutex);
    }
  }
  return NULL;
}

void
show_loop (avCtx *avctx, sdlCtx *sdlctx)
{
  showState state;
  state.running = true;
  
  AVFrame *outFrame = av_frame_alloc();
  outFrame->format = AV_PIX_FMT_YUYV422;
  outFrame->width = avctx->decoderCtx->width;
  outFrame->height = avctx->decoderCtx->height;
  av_frame_get_buffer(outFrame, 0);

  pthread_attr_t attr;
  struct sched_param param;
  param.sched_priority = sched_get_priority_min(SCHED_OTHER);
  int error;
  error = pthread_attr_init(&attr);
  assert(error == 0);
  error = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  assert(error == 0);
  error = pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  assert(error == 0);
  error = pthread_attr_setschedparam(&attr, &param);
  assert(error == 0);

  pthread_t sender_th;
  pthread_t reciver_th;

  thread_arg arg = {.avctx=avctx, .state=&state};

  error = pthread_create(&sender_th, &attr, show_sender, &arg);
  assert(error == 0);

  error = pthread_create(&reciver_th, &attr, show_reciver, &arg);
  assert(error == 0);
  
  while (1){
    showState_SDL_Event(&state);

    pthread_mutex_lock(&state.run_mutex);
    if (!state.running) {
      pthread_mutex_unlock(&state.run_mutex);
      break; // Exit the thread
    }
    pthread_mutex_unlock(&state.run_mutex);

    
    avCtx_convert_yuvy(avctx, outFrame);

    sdlCtx_update_mainTex (sdlctx, outFrame->data[0], outFrame->linesize[0],
			   outFrame->data[1], outFrame->linesize[1],
			   outFrame->data[2], outFrame->linesize[2]);

    sdlCtx_present (sdlctx);

    avCtx_next_frame(avctx);
  }


  pthread_cancel(sender_th);
  pthread_join(sender_th, NULL);

  pthread_cancel(reciver_th);
  pthread_join(reciver_th, NULL);
  
    
  av_frame_free(&outFrame);
}
