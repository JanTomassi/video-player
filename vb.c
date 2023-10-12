#include <SDL2/SDL_quit.h>
#include <SDL2/SDL_timer.h>
#include <assert.h>
#include <getopt.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <stdlib.h>
#include <string.h>
#include "avCtx.h"
#include "sdlCtx.h"
#include "show.h"

// #define LOG

int
main (int argc, char **argv)
{
  char opt = -1;
  char *devname = calloc (1, sizeof(char));
  char *video_size = calloc (1,sizeof(char));
  char *framerate = calloc (1, sizeof(char));
  while ((opt = getopt (argc, argv, "hd:f:r:")) != -1)
    switch (opt)
      {
      case 'h':
	printf("Usage: vb -d {file} -f {format} -r {rate}\nOnly -d is required\n");
	exit(0);
	break;
      case 'd':
        devname = realloc (devname, (strlen (optarg)) + 1);
        devname = strncpy (devname, optarg, strlen (optarg) + 1);
        break;
      case 'f':
	video_size = realloc (video_size, (strlen (optarg)) + 1);
        video_size = strncpy (video_size, optarg, strlen (optarg) + 1);
	break;
      case 'r':
	framerate = realloc (framerate, (strlen (optarg)) + 1);
        framerate = strncpy (framerate, optarg, strlen (optarg) + 1);
	break;
      default:
        exit (1);
      };

#ifdef LOG
  av_log_set_level (AV_LOG_DEBUG);
#else
  av_log_set_level (AV_LOG_INFO);
#endif

  avCtx *avctx = avCtx_init (devname, video_size, framerate);
  sdlCtx *sdlctx = sdlCtx_init(avctx->decoderCtx->width,
			       avctx->decoderCtx->height);
  
  show_loop(avctx, sdlctx);
  
  sdlCtx_free(&sdlctx);
  SDL_Quit();
  avCtx_free (&avctx);

  free (framerate);
  free (video_size);
  free (devname);
}
