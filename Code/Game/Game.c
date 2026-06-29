#include <SDK.h>

Init(Init)
{
    {
        EntResult Ent = EntInit();

        CompTransform Transform = {
            .Pos = V2Make(100.0f, 100.0f),
            .Size = V2Make(100.0f, 100.0f),
        };
        EntAddComp(Ent.ID, CompTransformHash, &Transform);

        CompRenderable Renderable = {
            .Type = RenderableType_Rect,
            .Rect = {
                .Color = Red,
                .Filled = True,
            }};
        EntAddComp(Ent.ID, CompRenderableHash, &Renderable);
    }
}

UpdateAndRender(UpdateAndRender)
{
    Iter It = IterInit((CompID[]){CompTransformHash}, 1);

    EntID EntID;
    CompTransform Transform;

    while (IterNext(&It, &EntID, &Transform))
    {
        Transform.Pos.X += 0.01f;
        EntAddComp(EntID, CompTransformHash, &Transform);
    }
}
