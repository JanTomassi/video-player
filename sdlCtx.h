#pragma once
#include <SDL2/SDL.h>


typedef struct
{
  SDL_Window   *win;
  SDL_Renderer *ren;
  SDL_Texture  *mainTex;
} sdlCtx;

sdlCtx *sdlCtx_init (int w, int h);
void sdlCtx_free (sdlCtx **var);
void sdlCtx_update_mainTex (sdlCtx *sdlctx, uint8_t *Yplain, int Ypitch,
                            uint8_t *Uplain, int Upitch, uint8_t *Vplain,
                            int Vpitch);
void sdlCtx_present (sdlCtx *sdlctx);
