#include "sdlCtx.h"
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <stdint.h>

#define DIE_SDL() die_sdl (__builtin_FUNCTION (), __builtin_LINE ())

__attribute__ ((noreturn)) static void
die_sdl (char *where, int line)
{
  fprintf(stderr, "\033[31;5mdie_sdl:{%s:%d}: %s\033[0m\n", where, line, SDL_GetError());
  exit(1);
}

static sdlCtx *
sdlCtx_allocate ()
{
  sdlCtx *new_inst = calloc (1, sizeof (sdlCtx));
  return new_inst;
}

__attribute_pure__ sdlCtx *
sdlCtx_init (int w, int h)
{
  sdlCtx *sdlctx = sdlCtx_allocate();

  sdlctx->win = SDL_CreateWindow ("Jan - GBlur", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED, w, h, //SDL_WINDOW_RESIZABLE |
				  SDL_WINDOW_OPENGL);
  if (sdlctx->win == NULL) DIE_SDL();

  sdlctx->ren = SDL_CreateRenderer(sdlctx->win, 0,
				   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (sdlctx->ren == NULL) DIE_SDL();

  sdlctx->mainTex = SDL_CreateTexture(sdlctx->ren, SDL_PIXELFORMAT_YUY2,
				      SDL_TEXTUREACCESS_STREAMING, w, h);
  if (sdlctx->mainTex == NULL) DIE_SDL();

  return sdlctx;
}

__attribute_pure__ void
sdlCtx_free (sdlCtx **var)
{
  sdlCtx *ptr = *var;
  
  SDL_DestroyTexture(ptr->mainTex);
  SDL_DestroyRenderer(ptr->ren);
  SDL_DestroyWindow(ptr->win);
  
  free(ptr);
  var = NULL;
}

void
sdlCtx_update_mainTex (sdlCtx *sdlctx, uint8_t *Yplain, int Ypitch,
                       uint8_t *Uplain, int Upitch, uint8_t *Vplain,
                       int Vpitch)
{
  int error = SDL_UpdateTexture (sdlctx->mainTex, NULL, Yplain, Ypitch);
  if (error < 0)
    DIE_SDL();
}

void
sdlCtx_present (sdlCtx *sdlctx)
{
  int error = 0;
  error = SDL_RenderClear(sdlctx->ren);
  if (error < 0)
    DIE_SDL();
  error = SDL_RenderCopy (sdlctx->ren, sdlctx->mainTex, NULL, NULL);
  if (error < 0)
    DIE_SDL();
  SDL_RenderPresent(sdlctx->ren);
}
