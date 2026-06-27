
#include "SDL.h"
#include <SDL3/SDL_error.h>

#define SegmentLength 2.0f

Bool RenderCircle(SDL_Renderer *Renderer, V2 Center, Float32 Radius)
{
	Assert(Renderer);
	Assert(Radius > 0.0f);

	Int32 N = (2 * SDL_PI_F * Radius) / SegmentLength; 
	SDL_FPoint Points[N + 1];

	Float32 AngleStep = (2.0f * SDL_PI_F) / N;

	for (Int32 I = 0; I <= N; I++)
	{
		Float32 Angle = I * AngleStep;
		
		Points[I].x = Center.X + (Radius * SDL_cosf(Angle));
		Points[I].y = Center.Y + (Radius * SDL_sinf(Angle));
	}

	return SDL_RenderLines(Renderer, Points, N);
}

Void Render(SDL *App)
{
    Assert(App);

    RenderBufIterate(&App->RenderBuf, Cmd)
    {
        switch (Cmd->Type)
        {
        case RenderCommand_Clear:
        {
            RenderClear *Clear = (RenderClear *)Cmd;

            if (!SDL_SetRenderDrawColor(App->Renderer, Clear->Color.R, Clear->Color.G, Clear->Color.B, Clear->Color.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
            if (!SDL_RenderClear(App->Renderer))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
        }
        break;

        case RenderCommand_DrawDebugText:
        {
            RenderDrawDebugText *DrawDebugText = (RenderDrawDebugText *)Cmd;

            if (!SDL_SetRenderDrawColor(App->Renderer, DrawDebugText->Color.R, DrawDebugText->Color.G, DrawDebugText->Color.B, DrawDebugText->Color.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
            if (!SDL_SetRenderScale(App->Renderer, DrawDebugText->Scale.X, DrawDebugText->Scale.Y))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }

            V2 Target = V2Div(DrawDebugText->Pos, DrawDebugText->Scale);
            if (!SDL_RenderDebugText(App->Renderer, Target.X, Target.Y, DrawDebugText->Str))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }

            if (!SDL_SetRenderScale(App->Renderer, 1.0f, 1.0f))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
        }
        break;

        case RenderCommand_DrawRect:
        {
            RenderDrawRect *DrawRect = (RenderDrawRect *)Cmd;

            SDL_FRect DstRect = {
                DrawRect->Dst.Pos.X,
                DrawRect->Dst.Pos.Y,
                DrawRect->Dst.Size.W,
                DrawRect->Dst.Size.H,
            };

            if (DrawRect->Tex == TexHandleInvalid)
            {
                // NOTE: Untextured Rectangle
                if (!SDL_SetRenderDrawColor(App->Renderer, DrawRect->Color.R, DrawRect->Color.G, DrawRect->Color.B, DrawRect->Color.A))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }

                if (DrawRect->Filled && !SDL_RenderFillRect(App->Renderer, &DstRect))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }
                else if (!SDL_RenderRect(App->Renderer, &DstRect))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }
            }
            else
            {
                // NOTE: Textured Rectangle
                if (DrawRect->Tex >= App->TexCount)
                {
                    LogCritical("Invalid texture handle: %d", DrawRect->Tex);
                    Assert(0);
                    break;
                }

                SDL_Texture *Tex = App->Texs[DrawRect->Tex];
                if (!Tex)
                {
                    LogCritical("Attempted to draw null texture at handle: %d", DrawRect->Tex);
                    Assert(0);
                    break;
                }

                SDL_SetTextureColorMod(Tex, DrawRect->Color.R, DrawRect->Color.G, DrawRect->Color.B);
                SDL_SetTextureAlphaMod(Tex, DrawRect->Color.A);

                SDL_FRect SrcRect = {
                    DrawRect->Src.Pos.X,
                    DrawRect->Src.Pos.Y,
                    DrawRect->Src.Size.W,
                    DrawRect->Src.Size.H,
                };

                SDL_FRect *SrcPtr = &SrcRect;
                if (DrawRect->Src.Pos.X == 0 && DrawRect->Src.Pos.Y == 0 && DrawRect->Src.Size.W == 0)
                {
                    SrcPtr = 0;
                }

                if (!SDL_RenderTexture(App->Renderer, Tex, SrcPtr, &DstRect))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }
            }
        }
        break;
        case RenderCommand_DrawCircle:
        {
            RenderDrawCircle *DrawCircle = (RenderDrawCircle *)Cmd;

            if (!SDL_SetRenderDrawColor(App->Renderer, DrawCircle->Color.R, DrawCircle->Color.G, DrawCircle->Color.B, DrawCircle->Color.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }

            if (!RenderCircle(App->Renderer, DrawCircle->Center, DrawCircle->Radius))
            {
            	LogCritical("%s", SDL_GetError());
            	Assert(0);
            }

        } break;
        case RenderCommand_DrawLine:
        {
            RenderDrawLine *DrawLine = (RenderDrawLine *)Cmd;

            if (!SDL_SetRenderDrawColor(App->Renderer, DrawLine->Color.R, DrawLine->Color.G, DrawLine->Color.B, DrawLine->Color.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
            if (!SDL_RenderLine(App->Renderer, DrawLine->Start.X, DrawLine->Start.Y, DrawLine->End.X, DrawLine->End.Y))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
        } break;

        case RenderCommand_None:
        default:
        {
            LogCritical("Unrecognized or not implemented render command (%d)", Cmd->Type);
            Assert(0);
        }
        break;
        }
    }

    SDL_RenderPresent(App->Renderer);
    RenderBufReset(&App->RenderBuf);
}
