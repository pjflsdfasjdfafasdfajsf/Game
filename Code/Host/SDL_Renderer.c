//
// NOTE: Renderer implementation with SDL_Renderer
//
#include "SDL_Renderer.h"

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

Renderer RendererInit(SDL_Window *Window)
{
    Assert(Window);

    Renderer Result = {0};

    Result.SDL = SDL_CreateRenderer(Window, 0);
    Result.Texs[ReservedCircleTex] = CreateCircleTex(Result.SDL, 256.0f, 3.0f);
    if (!Result.Texs[ReservedCircleTex])
    {
        Result.IsValid = False;
        return Result;
    }
    // NOTE: Do this since we have 0 reserved as 'no texture'.
    Result.TexCount++;

    if (!SDL_SetRenderLogicalPresentation(Result.SDL, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        Result.IsValid = False;
        return Result;
    }

    Result.IsValid = True;
    return Result;
}

// TODO: Lines are very long here.
Bool RendererDraw(World *World, Renderer *Renderer)
{
    Assert(Renderer);

    if (!SDL_SetRenderDrawColor(Renderer->SDL, 0, 0, 0, 0))
    {
        return False;
    }
    if (!SDL_RenderClear(Renderer->SDL))
    {
        return False;
    }

    CompID TransformID = CompIDInvalid;
    CompID RenderableID = CompIDInvalid;

    for (Uint32 I = 0; I < World->CompTypeCount; ++I)
    {
        // TODO: HACK
        if (World->CompSizes[I] == sizeof(CompTransform))
        {
            TransformID = I;
        }
        else if (World->CompSizes[I] == sizeof(CompRenderable))
        {
            RenderableID = I;
        }
    }

    for (EntID EntID = 0; EntID < MaxEnts; ++EntID)
    {
        if (!World->EntActive[EntID])
        {
            continue;
        }

        if (World->CompPresent[EntID][RenderableID])
        {
            CompRenderable *Renderable = (CompRenderable *)World->CompData[EntID][RenderableID];
            CompTransform *Transform = (CompTransform *)World->CompData[EntID][TransformID];

            switch (Renderable->Type)
            {
            case RenderableType_DebugText:
            {
                if (!SDL_SetRenderDrawColor(Renderer->SDL, Renderable->DebugText.Color.R, Renderable->DebugText.Color.G, Renderable->DebugText.Color.B, Renderable->DebugText.Color.A))
                {
                    return False;
                }
                if (!SDL_SetRenderScale(Renderer->SDL, Renderable->DebugText.Scale.X, Renderable->DebugText.Scale.Y))
                {
                    return False;
                }

                V2 Target = V2Div(Transform->Pos, Renderable->DebugText.Scale);
                if (!SDL_RenderDebugText(Renderer->SDL, Target.X, Target.Y, Renderable->DebugText.Str))
                {
                    return False;
                }

                if (!SDL_SetRenderScale(Renderer->SDL, 1.0f, 1.0f))
                {
                    return False;
                }
            }
            break;

            case RenderableType_Rect:
            {
                SDL_FRect DstRect = {
                    Transform->Pos.X,
                    Transform->Pos.Y,
                    Transform->Size.W,
                    Transform->Size.H,
                };

                if (Renderable->Rect.Tex == TexHandleInvalid)
                {
                    // NOTE: Untextured Rectangle
                    if (!SDL_SetRenderDrawColor(Renderer->SDL, Renderable->Rect.Color.R, Renderable->Rect.Color.G, Renderable->Rect.Color.B, Renderable->Rect.Color.A))
                    {
                        return False;
                    }

                    if (Renderable->Rect.Filled && !SDL_RenderFillRect(Renderer->SDL, &DstRect))
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
                    if (Renderable->Rect.Tex >= Renderer->TexCount)
                    {
                        SDL_SetError("Invalid texture handle: %d", Renderable->Rect.Tex);
                        return False;
                    }

                    SDL_Texture *Tex = Renderer->Texs[Renderable->Rect.Tex];
                    if (!Tex)
                    {
                        SDL_SetError("Attempted to draw null texture at handle: %d", Renderable->Rect.Tex);
                        return False;
                    }

                    SDL_SetTextureColorMod(Tex, Renderable->Rect.Color.R, Renderable->Rect.Color.G, Renderable->Rect.Color.B);
                    SDL_SetTextureAlphaMod(Tex, Renderable->Rect.Color.A);

                    SDL_FRect SrcRect = {
                        Renderable->Rect.Src.Pos.X,
                        Renderable->Rect.Src.Pos.Y,
                        Renderable->Rect.Src.Size.W,
                        Renderable->Rect.Src.Size.H,
                    };

                    SDL_FRect *SrcPtr = &SrcRect;
                    if (Renderable->Rect.Src.Pos.X == 0 && Renderable->Rect.Src.Pos.Y == 0 && Renderable->Rect.Src.Size.W == 0)
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
            case RenderableType_Circle:
            {
                SDL_Color Color = {
                    Renderable->Circle.Color.R,
                    Renderable->Circle.Color.G,
                    Renderable->Circle.Color.B,
                    Renderable->Circle.Color.A};
                if (!RenderCircle(Renderer->SDL, Renderer->Texs[ReservedCircleTex], Color, Transform->Pos, Renderable->Circle.Radius))
                {
                    return False;
                }
            }
            break;
            case RenderableType_Line:
            {
                if (!SDL_SetRenderDrawColor(Renderer->SDL, Renderable->Line.Color.R, Renderable->Line.Color.G, Renderable->Line.Color.B, Renderable->Line.Color.A))
                {
                    return False;
                }
                if (!SDL_RenderLine(Renderer->SDL, Transform->Pos.X, Transform->Pos.Y, Renderable->Line.End.X, Renderable->Line.End.Y))
                {
                    return False;
                }
            }
            break;

            case RenderableType_None:
            default:
            {
                SDL_SetError("Unrecognized or not implemented render command (%d)", Renderable->Type);
                return False;
            }
            break;
            }
        }
    }

    SDL_RenderPresent(Renderer->SDL);
    return True;
}
