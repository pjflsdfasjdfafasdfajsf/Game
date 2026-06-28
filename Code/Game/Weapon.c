
#include "Weapon.h"
#include "Math.h"
#include "State.h"

Void WeaponDraw(RenderBuf *RenderBuf, Weapon *Weapon, V2 Pos)
{
	Assert(Weapon);
	
	switch (Weapon->Type)
	{
	case WeaponType_Gun:
	{
		Rect Rect = RectZero;
		Rect.Size = Weapon->Size;
		Rect.Pos = Pos;
		RenderBufDrawRect(RenderBuf, Weapon->Tex, Rect, RectZero, Blue, Filled);
	} break;
	case WeaponType_Sword:
	{
		Rect Rect = RectZero;
		Rect.Size = Weapon->Size;
		Rect.Pos = Pos;
		RenderBufDrawRect(RenderBuf, Weapon->Tex, Rect, RectZero, Green, Filled);
	} break;
	// NOTE: handles WeaponTypeNone and unkown WeaponTypes
	default: break;
	}
}
