#pragma once

#include "storage.h"
#include "entity.h"

#include "angelscript.h"

#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>

#include <array>

/// Describes how to get physical input via SDL
struct RealInput {
	enum Type : uint32_t {
		NONE = 0,
		KEYBOARD,
		GAMEPAD_BUTTON,
		GAMEPAD_AXIS,
		MOUSE
	} type;

	union {
		SDL_Scancode key;
		struct {
			uint16_t gamepad;
			uint16_t button;
		} gbutton;
		struct {
			uint16_t gamepad;
			uint16_t axis;
		} gaxis;
		uint32_t mbutton;
		uint32_t _raw_;
	};

	inline RealInput() {
		type = NONE;
		_raw_ = 0;
	}

	inline void clear() {
		type = NONE;
		_raw_ = 0;
	}

	inline void operator = (const RealInput& other) {
		if (other.type == NONE) {
			clear();
		}
		else {
			*reinterpret_cast<uint64_t*>(this) = *reinterpret_cast<const uint64_t*>(&other);
		}
	}

	inline bool operator == (const RealInput& other) const {
		return *reinterpret_cast<const uint64_t*>(this) == *reinterpret_cast<const uint64_t*>(&other);
	}

	inline void set_key(SDL_Scancode key) {
		type = KEYBOARD;
		this->key = key;
	}

	inline void set_mouse(uint32_t mbutton) {
		type = MOUSE;
		this->mbutton = mbutton;
	}

	void set_button(int button);
	void set_axis(int axis);
	void set_hat(int hat);

	static const RealInput Empty;
};

static_assert(sizeof(RealInput) == sizeof(uint64_t), "RealInput must evaluate to 64-bit size");

class ButtonEventCallback {
private:
	asIScriptFunction* func;
	asIScriptObject* obj;

public:
	inline ButtonEventCallback() { func = nullptr; obj = nullptr; }

	void operator = (const ButtonEventCallback&) = delete;
	void operator = (ButtonEventCallback&&) = delete;

	void clear();
	void set_delegate(asIScriptFunction* delegate);
	void set_method(asIScriptObject* obj, asIScriptFunction* method);

	void operator() ();
};

/// The script-readable axis data
struct VirtualAxisState {
	std::array<RealInput, 3> bindings_positive;
	std::array<RealInput, 3> bindings_negative;

	float position;
	float velocity; // Instantaneous velocity
};

/// The script-readable button data
struct VirtualButtonState {
	std::array<RealInput, 3> bindings;

	ButtonEventCallback on_press;
	ButtonEventCallback on_release;

	bool state;
	bool pressed;
	bool released;
};

/// Controller class / metadata that defines what inputs are available and their names
struct VirtualController {
	Array<const char*> axis_names;
	Array<const char*> button_names;
};

/// An instance of a controller that holds the actual data that gets updated by the engine
struct ControllerInstance {
	const VirtualController* type;

	Array<VirtualAxisState> axes;
	Array<VirtualButtonState> buttons;
};

ControllerInstance* create_controller(const char* vcname, const char* instname);

void bind_controller(ControllerInstance* cont, asIScriptObject* comp);
void unbind_controller(ControllerInstance* cont);

/// Bind real inputs.
/// The rule is that no two inputs across all controller instances of the same type can share a binding
/// Unbind by passing RealInput::Empty to input
bool bind_button(ControllerInstance* cont, int b_index, RealInput input, int slot = -1);
bool bind_axis(ControllerInstance* cont, int a_index, int sign, RealInput input, int slot = -1);

void update_inputs(float dt);

ControllerInstance* get_controller_by_name(const char* name);

std::vector<ControllerInstance*> get_controllers_by_typename(const char* name);

void RegisterInputTypes(asIScriptEngine* engine);
int RegisterControllerTypes(asIScriptEngine* engine);

// TEMP
VirtualController* create_controller_type(const char* name, Array<const char*> axis_names, Array<const char*> button_names);