
#include "input.h"
#include "util.h"
#include "cstrkey.h"
#include "fileutil.h"
#include <new>
#include <vector>
#include <algorithm>
#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mouse.h>

#include "angelscript.h"
#include "scriptarray/scriptarray.h"

struct TypeEntry {
	const char* name;
	const VirtualController* type;
};

struct InstEntry {
	const char* name;
	ControllerInstance* inst;
};

static std::vector<TypeEntry> cont_types;
static std::vector<InstEntry> controllers;

const RealInput RealInput::Empty = RealInput();

static float get_axis_value(RealInput& input);
static bool get_button_value(RealInput& input);

// Think of ControllerInstances as variable-size structs. They should never be constructed manually.
static ControllerInstance* instantiate_controller(const VirtualController* ctype) {
	if (ctype == nullptr) return nullptr;

	// allocate one contiguous block of memory that is the exact size we need in total
	size_t n_axes = ctype->axis_names.size();
	size_t n_buttons = ctype->button_names.size();
	size_t memsize = sizeof(ControllerInstance) +
		n_axes * sizeof(VirtualAxisState) +
		n_buttons * (sizeof(VirtualButtonState));
	void* memblock = operator new (memsize);

	ControllerInstance* cont = (ControllerInstance*) memblock;
	VirtualAxisState* axes = new(cont + 1) VirtualAxisState[n_axes](); // ends with () to ensure initializers are called (i.e. all fields are zero)
	VirtualButtonState* buttons = new(axes + n_axes) VirtualButtonState[n_buttons]();

	new(&cont->axes) Array<VirtualAxisState>(axes, n_axes);
	new(&cont->buttons) Array<VirtualButtonState>(buttons, n_buttons);

	cont->type = ctype;

	return cont;
}

ControllerInstance* create_controller(const char* vcname, const char* instname) {
	const TypeEntry& type = *std::find_if(cont_types.begin(), cont_types.end(),
		[vcname](const TypeEntry& entry) -> bool { return strcmp(entry.name, vcname) == 0; }
	);

	const auto iter = std::find_if(controllers.begin(), controllers.end(),
		[instname](const InstEntry& entry) -> bool { return strcmp(entry.name, instname) == 0; }
	);

	ControllerInstance* inst = instantiate_controller(type.type);
	assert(inst != nullptr);
	if (iter == controllers.end()) {
		controllers.push_back({ copy(instname), inst });
	}
	else {
		InstEntry& entry = *iter;
		assert(entry.inst != nullptr);
		operator delete (entry.inst);
		entry.inst = inst;
	}
	return inst;
}

// TEMP for testing
VirtualController* create_controller_type(const char* name, Array<const char*> axis_names, Array<const char*> button_names) {
	const auto iter = std::find_if(cont_types.begin(), cont_types.end(),
		[name](const TypeEntry& entry) -> bool { return strcmp(entry.name, name) == 0; }
	);

	VirtualController* type = new VirtualController{
		std::move(axis_names),
		std::move(button_names)
	};

	if (iter == cont_types.end()) {
		cont_types.push_back({ copy(name), type });
	}
	else {
		TypeEntry& entry = *iter;
		delete entry.type;
		entry.type = type;
	}
	return type;
}

void update_inputs(float dt) {
	for (const InstEntry& entry : controllers) {
		ControllerInstance* inst = entry.inst;

		for (VirtualAxisState& axis : inst->axes) {
			float rawval = 0;

			for (RealInput& real : axis.bindings_negative) {
				rawval -= get_axis_value(real);
			}

			for (RealInput& real : axis.bindings_positive) {
				rawval += get_axis_value(real);
			}

			rawval = clamp(rawval, -1.f, 1.f);

			axis.velocity = (rawval - axis.position) / dt;
			axis.position = rawval;
		}

		int size = inst->buttons.size();
		for (int i = 0; i < size; ++i) {
			auto& button = inst->buttons[i];
			bool rawstate = false;

			for (RealInput& real : button.bindings) {
				rawstate = rawstate || get_button_value(real);
			}

			button.pressed = !button.state && rawstate;
			button.released = button.state && !rawstate;
			button.state = rawstate;

			if (button.pressed) {
				button.on_press();
			}
			else if (button.released) {
				button.on_release();
			}
		}
	}
}

void ButtonEventCallback::clear() {
	if (func != nullptr) {
		func->Release();
		func = nullptr;
	}
	if (obj != nullptr) {
		obj->Release();
		obj = nullptr;
	}
}

void ButtonEventCallback::set_delegate(asIScriptFunction* delegate) {
	assert(delegate != nullptr);
	if (func != nullptr) {
		func->Release();
	}
	if (obj != nullptr) {
		obj->Release();
		obj = nullptr;
	}
	func = delegate;
	func->AddRef();
}

void ButtonEventCallback::set_method(asIScriptObject* objref, asIScriptFunction* method) {
	if (func != nullptr) {
		func->Release();
	}
	if (obj != nullptr) {
		obj->Release();
	}
	assert(objref != nullptr && method != nullptr);

	func = method;
	obj = objref;

	func->AddRef();
	obj->AddRef();
}

void ButtonEventCallback::operator() () {
	if (func != nullptr) {
		asIScriptEngine* engine = func->GetEngine();
		asIScriptContext* ctx = engine->RequestContext();

		ctx->Prepare(func);

		if (obj != nullptr) {
			ctx->SetObject(obj);
		}

		int r = ctx->Execute();
		if (r == asEXECUTION_EXCEPTION) { // todo: propagate
			auto ex = GetExceptionDetails(ctx);
			printf("%s\n", ex.c_str());
		}

		ctx->Unprepare();
		engine->ReturnContext(ctx);
	}
}

void bind_controller(ControllerInstance* cont, asIScriptObject* comp) {
	asITypeInfo* cls = comp->GetObjectType();

	char declbuf[256];
	int n_buttons = cont->buttons.size();

	// Automatically populate callback bindings based on methods of the class that cares
	for (int i = 0; i < n_buttons; ++i) {
		auto& name = cont->type->button_names[i];

		sprintf(declbuf, "void on_press_%s()", name);
		asIScriptFunction* on_press = cls->GetMethodByDecl(declbuf);
		if (on_press != nullptr) {
			cont->buttons[i].on_press.set_method(comp, on_press);
		}

		sprintf(declbuf, "void on_release_%s()", name);
		asIScriptFunction* on_release = cls->GetMethodByDecl(declbuf);
		if (on_release != nullptr) {
			cont->buttons[i].on_release.set_method(comp, on_release);
		}
	}
}

void unbind_controller(ControllerInstance* cont) {
	for (VirtualButtonState& button : cont->buttons) {
		button.on_press.clear();
		button.on_release.clear();
	}
}

bool bind_button(ControllerInstance* cont, int b_index, RealInput input, int slot) {
	// clear matching bindings on controllers of the same type
	if (input.type != RealInput::NONE) {
		for (auto& entry : controllers) {
			if (cont->type != entry.inst->type) continue;

			for (auto& axis : entry.inst->axes) {
				for (auto& binding : axis.bindings_negative) {
					if (binding == input) {
						binding.clear();
						// early break via (shudder) goto?
					}
				}

				for (auto& binding : axis.bindings_positive) {
					if (binding == input) {
						binding.clear();
					}
				}
			}

			for (auto& button : entry.inst->buttons) {
				for (auto& binding : button.bindings) {
					if (binding == input) {
						binding.clear();
					}
				}
			}
		}
	}

	if (slot >= 0) {
		cont->buttons[b_index].bindings[slot] = input;
		return true;
	}
	else {
		for (auto& binding : cont->buttons[b_index].bindings) {
			if (binding.type == RealInput::NONE) {
				binding = input;
				return true;
			}
		}
		return false;
	}
}

bool bind_axis(ControllerInstance* cont, int a_index, int sign, RealInput input, int slot) {
	if (sign == 0) return false;

	// clear matching bindings on controllers of the same type
	if (input.type != RealInput::NONE) {
		for (auto& entry : controllers) {
			if (cont->type != entry.inst->type) continue;

			for (auto& axis : entry.inst->axes) {
				for (auto& binding : axis.bindings_negative) {
					if (binding == input) {
						binding.clear();
						// early break via (shudder) goto?
					}
				}

				for (auto& binding : axis.bindings_positive) {
					if (binding == input) {
						binding.clear();
					}
				}
			}

			for (auto& button : entry.inst->buttons) {
				for (auto& binding : button.bindings) {
					if (binding == input) {
						binding.clear();
					}
				}
			}
		}
	}

	if (slot >= 0) {
		if (sign < 0) {
			cont->axes[a_index].bindings_negative[slot];
		}
		else {
			cont->axes[a_index].bindings_positive[slot];
		}
		return true;
	}
	else {
		if (sign < 0) {
			for (auto& binding : cont->axes[a_index].bindings_negative) {
				if (binding.type == RealInput::NONE) {
					binding = input;
					return true;
				}
			}
		}
		else {
			for (auto& binding : cont->axes[a_index].bindings_positive) {
				if (binding.type == RealInput::NONE) {
					binding = input;
					return true;
				}
			}
		}
		return false;
	}
}

ControllerInstance* get_controller_by_name(const char* name) {
	auto cont_iter = std::find_if(controllers.begin(), controllers.end(),
		[name](const InstEntry& entry) -> bool { return strcmp(entry.name, name) == 0; }
	);

	if (cont_iter == controllers.end()) {
		return nullptr;
	}
	else {
		return cont_iter->inst;
	}
}

std::vector<ControllerInstance*> get_controllers_by_typename(const char* name) {
	std::vector<ControllerInstance*> result;

	auto type_iter = std::find_if(cont_types.begin(), cont_types.end(),
		[name](const TypeEntry& entry) -> bool { return strcmp(entry.name, name) == 0; }
	);

	if (type_iter == cont_types.end()) return result;

	const VirtualController* type = type_iter->type;

	for (auto& entry : controllers) {
		if (entry.inst->type == type) {
			result.push_back(entry.inst);
		}
	}

	return result;
}

static float get_axis_value(RealInput& input) {
	switch (input.type) {
	case RealInput::NONE:
		return 0.f;
	case RealInput::KEYBOARD:
		return SDL_GetKeyboardState(nullptr)[input.key] ? 1.f : 0.f;
	case RealInput::GAMEPAD_AXIS:
	{

	}
	break;
	case RealInput::GAMEPAD_BUTTON:
	{

	}
	break;
	case RealInput::MOUSE:
		return SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(input.mbutton) ? 1.f : 0.f;
	default:
	break;
	};

	return 0.f;
}

static bool get_button_value(RealInput& input) {
	switch (input.type) {
	case RealInput::NONE:
		return 0.f;
	case RealInput::KEYBOARD:
		return SDL_GetKeyboardState(nullptr)[input.key];
	case RealInput::GAMEPAD_AXIS:
	{

	}
	break;
	case RealInput::GAMEPAD_BUTTON:
	{

	}
	break;
	case RealInput::MOUSE:
		return SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(input.mbutton);
	default:
		break;
	};

	return false;
}

Result<> init_controller_types(FILE* stream) {
	try {
		int n_controllers = read<uint16_t>(stream);

		for (int i = 0; i < n_controllers; ++i) {
			const char* name = read_string(stream, read<uint16_t>(stream));
			Array<const char*> axes(read<uint16_t>(stream));
			for (auto& n : axes) {
				n = read_string(stream, read<uint16_t>(stream));
			}
			Array<const char*> buttons(read<uint16_t>(stream));
			for (auto& n : buttons) {
				n = read_string(stream, read<uint16_t>(stream));
			}
			create_controller_type(name, axes, buttons);
		}

		return Result<>::success;
	}
	catch (Error& e) {
		return e;
	}
}

static void SetOnPressCallback(VirtualButtonState* button, asIScriptFunction* callback) {
	button->on_press.set_delegate(callback);
}

static void SetOnReleaseCallback(VirtualButtonState* button, asIScriptFunction* callback) {
	button->on_release.set_delegate(callback);
}

static asITypeInfo* strarray_type;

void RegisterInputTypes(asIScriptEngine* engine) {
	strarray_type = engine->GetTypeInfoByDecl("array<string>");

	int r;

	r = engine->RegisterObjectType("InputAxis", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	r = engine->RegisterObjectProperty("InputAxis", "const float position", asOFFSET(VirtualAxisState, position)); assert(r >= 0);
	r = engine->RegisterObjectProperty("InputAxis", "const float velocity", asOFFSET(VirtualAxisState, velocity)); assert(r >= 0);

	r = engine->RegisterObjectType("InputButton", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	r = engine->RegisterObjectProperty("InputButton", "const bool state", asOFFSET(VirtualButtonState, state)); assert(r >= 0);
	r = engine->RegisterObjectProperty("InputButton", "const bool pressed", asOFFSET(VirtualButtonState, pressed)); assert(r >= 0);
	r = engine->RegisterObjectProperty("InputButton", "const bool released", asOFFSET(VirtualButtonState, released)); assert(r >= 0);

	r = engine->RegisterFuncdef("void ButtonEventCallback()"); assert(r >= 0);

	r = engine->RegisterObjectMethod("InputButton", "void bind_on_press(ButtonEventCallback@)",
		asFUNCTION(SetOnPressCallback), asCALL_CDECL_OBJFIRST); assert(r > 0);
	r = engine->RegisterObjectMethod("InputButton", "void bind_on_release(ButtonEventCallback@)",
		asFUNCTION(SetOnReleaseCallback), asCALL_CDECL_OBJFIRST); assert(r > 0);
}

// WARNING: these are HIGHLY dependent on instantiating controllers properly
static int GetButtonOffset(const VirtualController* type, int index) {
	return sizeof(ControllerInstance) +
		sizeof(VirtualAxisState) * type->axis_names.size() +
		sizeof(VirtualButtonState) * index;
}

static int GetAxisOffset(const VirtualController* type, int index) {
	return sizeof(ControllerInstance) +
		sizeof(VirtualAxisState) * index;
}
// So never instantiate them by hand and do not make a constructor. Constructors don't allocate memory the way this expects.

class GetController {
	const VirtualController* const type;

public:
	GetController(const VirtualController* type) : type(type) {}

	ControllerInstance* operator () (int index) {
		for (auto& cont : controllers) {
			if (cont.inst->type != type) continue;

			if (index-- <= 0) {
				return cont.inst;
			}
		}
		return nullptr;
	}
};

CScriptArray* GetAxisNames(ControllerInstance* inst) {
	int n_axes = inst->type->axis_names.size();
	CScriptArray* arr = CScriptArray::Create(strarray_type, n_axes);

	for (int i = 0; i < n_axes; ++i) {
		arr->SetValue(i, &std::string(inst->type->axis_names[i]));
	}

	return arr;
}

CScriptArray* GetButtonNames(ControllerInstance* inst) {
	int n_axes = inst->type->button_names.size();
	CScriptArray* arr = CScriptArray::Create(strarray_type, n_axes);

	for (int i = 0; i < n_axes; ++i) {
		arr->SetValue(i, &std::string(inst->type->button_names[i]));
	}

	return arr;
}

VirtualAxisState* GetAxisByName(ControllerInstance* inst, const std::string& name) {
	int i = 0;
	for (const char* axis_name : inst->type->axis_names) {
		if (name == axis_name) {
			return &inst->axes[i];
		}
		++i;
	}
	return nullptr;
}

VirtualButtonState* GetButtonByName(ControllerInstance* inst, const std::string& name) {
	int i = 0;
	for (const char* button_name : inst->type->button_names) {
		if (name == button_name) {
			return &inst->buttons[i];
		}
		++i;
	}
	return nullptr;
}

VirtualAxisState* GetAxisByIndex(ControllerInstance* inst, int index) {
	if (index >= 0 && index < inst->axes.size()) {
		return &inst->axes[index];
	}
	else {
		return nullptr;
	}
}

VirtualButtonState* GetButtonByIndex(ControllerInstance* inst, int index) {
	if (index >= 0 && index < inst->buttons.size()) {
		return &inst->buttons[index];
	}
	else {
		return nullptr;
	}
}

#define check(EXPR) {int r = (EXPR); if (r < 0) return r;} do {} while(0)
static int RegisterControllerType(asIScriptEngine* engine, const VirtualController* vcont,
		const char* const name, void* const place) {
	char declbuf[256];

	check(engine->RegisterObjectType(name, 0, asOBJ_REF | asOBJ_NOCOUNT));

	int n_axes = vcont->axis_names.size();
	for (int i = 0; i < n_axes; ++i) {
		sprintf(declbuf, "InputAxis %s", vcont->axis_names[i]);
		check(engine->RegisterObjectProperty(name, declbuf, GetAxisOffset(vcont, i)));
	}

	int n_buttons = vcont->button_names.size();
	for (int i = 0; i < n_buttons; ++i) {
		sprintf(declbuf, "InputButton %s", vcont->button_names[i]);
		check(engine->RegisterObjectProperty(name, declbuf, GetButtonOffset(vcont, i)));
	}

	check(engine->RegisterObjectMethod(name, "void bind(EntityComponent@)",
		asFUNCTION(bind_controller), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod(name, "void unbind()",
		asFUNCTION(unbind_controller), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod(name, "array<string>@ get_axis_names()",
		asFUNCTION(GetAxisNames), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod(name, "array<string>@ get_button_names()",
		asFUNCTION(GetButtonNames), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod(name, "InputAxis@ axis(int)",
		asFUNCTION(GetAxisByIndex), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod(name, "InputAxis@ axis(const string &in)",
		asFUNCTION(GetAxisByName), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod(name, "InputButton@ button(int)",
		asFUNCTION(GetButtonByIndex), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod(name, "InputButton@ button(const string &in)",
		asFUNCTION(GetButtonByName), asCALL_CDECL_OBJFIRST));

	GetController* getter = new(place) GetController(vcont);

	sprintf(declbuf, "%s@ get_%s_instance(int)", name, name);
	check(engine->RegisterGlobalFunction(declbuf, asMETHOD(GetController, operator ()),
		asCALL_THISCALL_ASGLOBAL, (void*) getter));

	return 0;
}

int RegisterControllerTypes(asIScriptEngine* engine) {
	GetController** getters = new GetController*[cont_types.size()]; // this will never be freed, but that's okay

	check(engine->SetDefaultNamespace("Input"));
	for (TypeEntry& entry : cont_types) {
		check(RegisterControllerType(engine, entry.type, entry.name, (void*) getters));
		++getters;
	}
	check(engine->SetDefaultNamespace(""));

	return 0;
}