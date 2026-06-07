
#pragma once

#include "game_types.h"

static inline vector2 vector2_add(vector2 a, vector2 b) {
	return (vector2){
		.x = a.x + b.x,
		.y = a.y + b.y,
	};
}
