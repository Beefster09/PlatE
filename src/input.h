#pragma once

#include "storage.h"
#include "direction.h"
#include <SDL2\SDL_keyboard.h>
#include <SDL2\SDL_keycode.h>

/// The script-readable axis data
struct VirtualAxis {
	const char* name;
	float position;
	float last_pos;
	float velocity; // Instantaneous velocity
	int last_poll; // needed to calculate velocity
};

/// The script-readable button data
struct VirtualButton {
	const char* name;
	int last_press;

	bool state : 1;
	bool last_state : 1;
	bool pressed : 1;
	bool released : 1;
};

/// Describes how to get physical input managed by SDL
struct RealInput {
	enum Type {
		KEYBOARD,
		GAMEPAD_BUTTON,
		GAMEPAD_AXIS,
		GAMEPAD_HAT,
		MOUSE
	} type;

	union {
		SDL_Scancode key;
		struct {
			short gamepad;
			short button;
		} gbutton;
		struct {
			short gamepad;
			short axis;
		} gaxis;
		struct {
			short gamepad;
			char hat;
			char dir;
		} ghat;
	};
};

/// The bridge between real and virtual inputs (engine-managed)
struct InputBinding {
	RealInput input;
	enum {
		BUTTON,
		AXIS_PLUS,
		AXIS_MINUS
	} type;
	union {
		VirtualButton* button;
		VirtualAxis* axis;
	};
};

/// Controller with inputs.
/// Can be attached to different engine pieces (including Entities and GUI elements)
/// Accessible by scripts
struct VirtualController {
	const char* name;
	VirtualAxis x, y;
	Array<VirtualAxis> axes;
	Array<VirtualButton> buttons;
	void* attachment; // TODO: make into tagged union
	bool enabled;
};

struct InputEvent {
	enum Type {
		BUTTON_PRESS,
		BUTTON_RELEASE,
		DIRECTION_PRESS,
		DIRECTION_RELEASE,
		AXIS_TAP
	};
};

void bind_button(DenseBucket<InputBinding>& bindings, RealInput& real, VirtualController& virt, int index);
void bind_axis(DenseBucket<InputBinding>& bindings, RealInput& real, VirtualController& virt, int sign, int index);

void update_virtual_controllers(DenseBucket<InputBinding>& bindings, Array<VirtualController*>& controllers);

//struct InputElement {
//	enum {
//		NOTHING,
//		BUTTON,
//		BUTTON_PRESS,
//		BUTTON_RELEASE,
//		DIRECTION,
//		DIRECTION_PRESS,
//		DIRECTION_RELEASE,
//		AXIS_NEUTRAL,
//		AXIS_TILT,
//		AXIS_TAP   // Tap/smash inputs
//	} type;
//
//	union {
//		int button;
//		Direction dir;
//		struct {
//			short index;
//			short sign;
//		} axis;
//	};
//};
//
///// Fightstick input patterns
//struct InputPattern {
//	uint32_t window; // ms window to input the pattern
//	uint32_t cancel_filter; // types that break the pattern
//	Array<InputElement> sequence;
//	bool relative_to_facing; /// If true, interpret right as forward and left as back.
//	// This raises some implications requiring facing direction to be built-in and
//	// somehow accessible from the pattern matcher
//};

//void bind_pattern(void* bindings, InputPattern& pattern, void* callback);
//InputPattern parse_pattern(const char* pattern_str);