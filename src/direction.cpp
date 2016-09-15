
#include "direction.h"
#include <cmath>

Direction operator - (Direction dir) {
	return (Direction) -(int) dir;
}

Direction operator & (Direction a, Direction b) {
	if (a == b) return a;
	if (a == -b) return Direction::None;
	if (a == Direction::None || b == Direction::None) return Direction::None;

	int x = x_sign(a) + x_sign(b);
	int y = y_sign(a) + y_sign(b);
	if (abs(x) == 1) x = 0;
	if (abs(y) == 1) y = 0;
	return dir_of(x, y);
}

Direction operator + (Direction a, Direction b) {
	if (a == b) return a;
	if (a == -b) return Direction::None;
	if (a == Direction::None) return b;
	if (b == Direction::None) return a;

	int x = x_sign(a) + x_sign(b);
	int y = y_sign(a) + y_sign(b);
	return dir_of(x, y);
}

Direction dir_of(int x, int y) {
	if (x < 0) {
		if (y < 0) {
			return Direction::UpLeft;
		}
		else if (y > 0) {
			return Direction::DownLeft;
		}
		else {
			return Direction::Left;
		}
	}
	else if (x > 0) {
		if (y < 0) {
			return Direction::UpRight;
		}
		else if (y > 0) {
			return Direction::DownRight;
		}
		else {
			return Direction::Right;
		}
	}
	else {
		if (y < 0) {
			return Direction::Up;
		}
		else if (y > 0) {
			return Direction::Down;
		}
		else {
			return Direction::None;
		}
	}
}

int x_sign(Direction dir) {
	switch (dir) {
	case Direction::Left: 
	case Direction::UpLeft: 
	case Direction::DownLeft:
		return -1;
	case Direction::Right:
	case Direction::UpRight:
	case Direction::DownRight:
		return 1;
	case Direction::None:
	case Direction::Up:
	case Direction::Down:
	default:
		return 0;
	}
}

int y_sign(Direction dir) {
	switch (dir) {
	case Direction::UpLeft:
	case Direction::Up:
	case Direction::UpRight:
		return -1;
	case Direction::DownLeft:
	case Direction::Down:
	case Direction::DownRight:
		return 1;
	case Direction::Left:
	case Direction::None:
	case Direction::Right:
	default:
		return 0;
	}
}

bool is_cardinal(Direction dir) {
	return dir != Direction::None && abs((int)dir) <= 2;
}