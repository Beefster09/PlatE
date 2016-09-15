#pragma once

#include <cstdint>

// Cardinal directions / quadrants
enum class Direction: int8_t {
	None = 0,
	Up   = -1, Down  = 1,
	Left = -2, Right = 2,
	UpLeft   = -3, UpRight   = 4,
	DownLeft = -4, DownRight = 3
};

// Opposite direction
Direction operator - (Direction dir);

// Intersection of directions
// Returns greatest commonality of the 4 cardinal directions
// ex: UpLeft & DownLeft => Left
// ex: DownRight & Left => None
// ex: Up & UpLeft => Up
// ex: Left & Right => None
// ex: DownLeft & DownLeft => DownLeft
Direction operator & (Direction a, Direction b);

// Add 2 directions
// Opposite directions cancel out
// Same directions give the same
Direction operator + (Direction a, Direction b);

// Direction of a vector
Direction dir_of(int x, int y);

int x_sign(Direction dir);
int y_sign(Direction dir);

bool is_cardinal(Direction dir);