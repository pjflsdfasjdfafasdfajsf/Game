//
// NOTE: Renderer implementation with SDL_Renderer
//
#include "SDL_Renderer.h"
#include "Mem.h"

//
// NOTE: Internal
//

SDL_Texture *CreateCircleTex(SDL_Renderer *Renderer, Float32 Radius, Float32 Thickness)
{
    Assert(Renderer);
    Assert(Radius > 0.0f);

    Int32 Size = (Int32)(Radius * 2.0f);

    SDL_Surface *Surface = SDL_CreateSurface(Size, Size, SDL_PIXELFORMAT_RGBA8888);
    if (!Surface)
    {
        return 0;
    }

    Uint32 *Pixels = Surface->pixels;

    V2 Center = V2Make(Radius, Radius);
    Float32 HalfThickness = Thickness / 2.0f;
    // NOTE: Center radius of the stroke ring
    Float32 StrokeCenter = Radius - (HalfThickness);

    for (Int32 Y = 0; Y < Size; Y++)
    {
        for (Int32 X = 0; X < Size; X++)
        {
            V2 Pixel = V2Make((Float32)X + 0.5f, (Float32)Y + 0.5f);
            V2 Delta = V2Sub(Pixel, Center);

            Float32 RelDist = V2Len(Delta);
            Float32 AbsDist = SDL_fabsf(RelDist - StrokeCenter);

            Uint8 A = 0;
            if (AbsDist < HalfThickness - 0.5f)
            {
                A = 255;
            }
            else if (AbsDist < HalfThickness + 0.5f)
            {
                Float32 Fact = (HalfThickness + 0.5f) - AbsDist;
                A = (Uint8)(Fact * 255.0f);
            }

            Pixels[Y * Size + X] = SDL_MapSurfaceRGBA(Surface, 255, 255, 255, A);
        }
    }
    SDL_Texture *Result = SDL_CreateTextureFromSurface(Renderer, Surface);
    SDL_DestroySurface(Surface);

    if (Result)
    {
        SDL_SetTextureScaleMode(Result, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(Result, SDL_BLENDMODE_BLEND);
    }

    return Result;
}

Bool RenderCircle(SDL_Renderer *Renderer, SDL_Texture *Tex, SDL_Color Color, V2 Center, Float32 Radius)
{
    Assert(Renderer);
    Assert(Tex);
    Assert(Radius > 0.0f);

    SDL_SetTextureColorMod(Tex, Color.r, Color.g, Color.b);
    SDL_SetTextureAlphaMod(Tex, Color.a);

    SDL_FRect DestRect = {
        Center.X - Radius,
        Center.Y - Radius,
        Radius * 2.0f,
        Radius * 2.0f,
    };
    return SDL_RenderTexture(Renderer, Tex, 0, &DestRect);
}

//
// NOTE: Implementation
//

Renderer RendererInit(SDL_Window *Window, MemAlloc Alloc)
{
    Assert(Window);

    Renderer Result = {0};

    Result.SDL = SDL_CreateRenderer(Window, 0);
    Result.Texs[Result.TexCount++] = CreateCircleTex(Result.SDL, 256.0f, 3.0f);
    if (!Result.Texs[ReservedCircleTex])
    {
        Result.IsValid = False;
        return Result;
    }

    if (!SDL_SetRenderLogicalPresentation(Result.SDL, InternalRes.W, InternalRes.H, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        Result.IsValid = False;
        return Result;
    }

    Result.Buf = RenderBufInit(&Alloc, Kb(32));
    if (!Result.Buf.IsValid)
    {
        SDL_SetError("Failed to initialize render buffer");

        Result.IsValid = False;
        return Result;
    }

    Result.IsValid = True;
    return Result;
}

// TODO: Better error handling
Bool RendererDraw(Renderer *Renderer)
{
    Assert(Renderer);

    RenderBufIterate(&Renderer->Buf, Cmd)
    {
        switch (Cmd->Type)
        {
        case RenderCommand_Clear:
        {
            RenderClear *Clear = (RenderClear *)Cmd;

            if (!SDL_SetRenderDrawColor(Renderer->SDL, Clear->Color.R, Clear->Color.G, Clear->Color.B, Clear->Color.A))
            {
                return False;
            }
            if (!SDL_RenderClear(Renderer->SDL))
            {
                return False;
            }
        }
        break;

        case RenderCommand_DrawDebugText:
        {
            RenderDrawDebugText *DrawDebugText = (RenderDrawDebugText *)Cmd;

            if (!SDL_SetRenderDrawColor(Renderer->SDL, DrawDebugText->Color.R, DrawDebugText->Color.G, DrawDebugText->Color.B, DrawDebugText->Color.A))
            {
                return False;
            }
            if (!SDL_SetRenderScale(Renderer->SDL, DrawDebugText->Scale.X, DrawDebugText->Scale.Y))
            {
                return False;
            }

            V2 Target = V2Div(DrawDebugText->Pos, DrawDebugText->Scale);
            if (!SDL_RenderDebugText(Renderer->SDL, Target.X, Target.Y, DrawDebugText->Str))
            {
                return False;
            }

            if (!SDL_SetRenderScale(Renderer->SDL, 1.0f, 1.0f))
            {
                return False;
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
                if (!SDL_SetRenderDrawColor(Renderer->SDL, DrawRect->Color.R, DrawRect->Color.G, DrawRect->Color.B, DrawRect->Color.A))
                {
                    return False;
                }

                if (DrawRect->Filled && !SDL_RenderFillRect(Renderer->SDL, &DstRect))
                {
                    return False;
                }
                else if (!SDL_RenderRect(Renderer->SDL, &DstRect))
                {
                    return False;
                }
            }
            else
            {
                // NOTE: Textured Rectangle
                if (DrawRect->Tex >= Renderer->TexCount)
                {
                    SDL_SetError("Invalid texture handle: %d", DrawRect->Tex);
                    return False;
                }

                SDL_Texture *Tex = Renderer->Texs[DrawRect->Tex];
                if (!Tex)
                {
                    SDL_SetError("Attempted to draw null texture at handle: %d", DrawRect->Tex);
                    return False;
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

                if (!SDL_RenderTexture(Renderer->SDL, Tex, SrcPtr, &DstRect))
                {
                    return False;
                }
            }
        }
        break;
        case RenderCommand_DrawCircle:
        {
            RenderDrawCircle *DrawCircle = (RenderDrawCircle *)Cmd;

            SDL_Color Color = {
                DrawCircle->Color.R,
                DrawCircle->Color.G,
                DrawCircle->Color.B,
                DrawCircle->Color.A};
            if (!RenderCircle(Renderer->SDL, Renderer->Texs[ReservedCircleTex], Color, DrawCircle->Center, DrawCircle->Radius))
            {
                return False;
            }
        }
        break;
        case RenderCommand_DrawLine:
        {
            RenderDrawLine *DrawLine = (RenderDrawLine *)Cmd;

            if (!SDL_SetRenderDrawColor(Renderer->SDL, DrawLine->Color.R, DrawLine->Color.G, DrawLine->Color.B, DrawLine->Color.A))
            {
                return False;
            }
            if (!SDL_RenderLine(Renderer->SDL, DrawLine->Start.X, DrawLine->Start.Y, DrawLine->End.X, DrawLine->End.Y))
            {
                return False;
            }
        }
        break;

        case RenderCommand_None:
        default:
        {
            SDL_SetError("Unrecognized or not implemented render command (%d)", Cmd->Type);
            return False;
        }
        break;
        }
    }

    SDL_RenderPresent(Renderer->SDL);
    RenderBufReset(&Renderer->Buf);

    return True;
}
