
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

static void bind_spec_pos(ControllerInstance* inst, int axis, const char* spec);
static void bind_spec_neg(ControllerInstance* inst, int axis, const char* spec);
static void bind_spec(ControllerInstance* inst, int button, const char* spec);

#pragma region Bookkeeping

// Think of ControllerInstances as variable-size structs. They should never be constructed manually.
static ControllerInstance* instantiate_controller(const VirtualController* ctype) {
	if (ctype == nullptr) return nullptr;

	// allocate one contiguous block of memory that is the exact size we need in total
	size_t n_axes = ctype->axis_names.size();
	size_t n_buttons = ctype->button_names.size();
	size_t memsize = sizeof(ControllerInstance)
		           + sizeof(VirtualAxisState)   * n_axes
		           + sizeof(VirtualButtonState) * n_buttons;
	void* memblock = malloc(memsize);
	// need to zero memory for axes and buttons since we won't immediately set everything yet.
	memset(memblock, 0, memsize);

	ControllerInstance* cont = (ControllerInstance*) memblock;
	VirtualAxisState* axes = reinterpret_cast<VirtualAxisState*>(cont + 1);
	VirtualButtonState* buttons = reinterpret_cast<VirtualButtonState*>(axes + n_axes);

	assert((char*) (buttons + n_buttons) == (char*) memblock + memsize);

	if (n_axes > 0) {
		new(&cont->axes) Array<VirtualAxisState>(axes, n_axes);
	}
	else {
		new(&cont->axes) Array<VirtualAxisState>();
	}

	if (n_buttons > 0) {
		new(&cont->buttons) Array<VirtualButtonState>(buttons, n_buttons);
	}
	else {
		new(&cont->buttons) Array<VirtualButtonState>();
	}

	cont->type = ctype;

	return cont;
}

ControllerInstance* create_controller(const VirtualController* vctype, const char* instname) {
	const auto iter = std::find_if(controllers.begin(), controllers.end(),
		[instname](const InstEntry& entry) -> bool { return strcmp(entry.name, instname) == 0; }
	);

	ControllerInstance* inst = instantiate_controller(vctype);
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

	name = copy(name);
	VirtualController* type = new VirtualController{
		name,
		std::move(axis_names),
		std::move(button_names)
	};

	if (iter == cont_types.end()) {
		cont_types.push_back({ name, type });
	}
	else {
		ERR("WARNING: controller '%s' redefined", name);
		TypeEntry& entry = *iter;
		for (const char* name : entry.type->axis_names) delete[] name;
		for (const char* name : entry.type->button_names) delete[] name;
		const_cast<Array<const char*>&>(entry.type->axis_names).free();
		const_cast<Array<const char*>&>(entry.type->button_names).free();
		delete entry.name;
		delete entry.type;
		entry.type = type;
	}
	return type;
}

const VirtualController* get_controller_type_by_name(const char* name) {
	auto type_iter = std::find_if(cont_types.begin(), cont_types.end(),
		[name](const TypeEntry& entry) -> bool { return strcmp(entry.name, name) == 0; }
	);

	if (type_iter == cont_types.end()) return nullptr;
	else return type_iter->type;
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

	const VirtualController* type = get_controller_type_by_name(name);
	if (type == nullptr) return result;

	for (auto& entry : controllers) {
		if (entry.inst->type == type) {
			result.push_back(entry.inst);
		}
	}

	return result;
}

Result<> init_controller_types(FILE* stream) {
	try {
		int n_controllers = read<uint16_t>(stream);

		for (int i = 0; i < n_controllers; ++i) {
			const char* name = read_string<uint16_t>(stream);
			Array<const char*> axes(read<uint16_t>(stream));
			for (auto& n : axes) {
				n = read_string<uint16_t>(stream);
			}
			Array<const char*> buttons(read<uint16_t>(stream));
			for (auto& n : buttons) {
				n = read_string<uint16_t>(stream);
			}
			create_controller_type(name, axes, buttons);
		}

		return Result<>::success;
	}
	catch (Error& e) {
		return e;
	}
}

Result<> init_controllers(FILE* stream) {
	try {
		int n_controllers = read<uint16_t>(stream);

		for (int i = 0; i < n_controllers; ++i) {
			const char* name = read_string<uint16_t>(stream);
			const char* tname = read_string<uint16_t>(stream);

			const VirtualController* type = get_controller_type_by_name(tname);
			if (type == nullptr) return Error(Errors::NoSuchControllerType, tname);

			ControllerInstance* inst = create_controller(type, name);

			char buffer[256];

			auto n_axes = type->axis_names.size();
			for (int axis = 0; axis < n_axes; ++axis) {
				bind_spec_pos(inst, axis, read_string<uint16_t>(stream, buffer));
				bind_spec_neg(inst, axis, read_string<uint16_t>(stream, buffer));
			}

			auto n_btns = type->button_names.size();
			for (int btn = 0; btn < n_btns; ++btn) {
				bind_spec(inst, btn, read_string<uint16_t>(stream, buffer));
			}
		}
		return Result<>::success;
	}
	catch (Error& e) {
		return e;
	}
}

#pragma endregion

#pragma region InputUpdates

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

#pragma endregion

#pragma region Callbacks

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

#pragma endregion

#pragma region Config

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

static const char* read_real_input(const char* spec, RealInput& input) {
	if (spec == nullptr) {
		input.clear();
		return nullptr;
	}

	char buffer[32];
	const char* end = strchr(spec, ',');
	const char* ret;
	if (end == nullptr) {
		strcpy(buffer, spec); // unnecessary, but is fine since this isn't run frequently
		ret = nullptr;
	}
	else {
		strncpy(buffer, spec, end - spec);
		buffer[end - spec] = 0;
		ret = end + 1;
		// skip over spaces
		while (isspace(*ret)) {
			++ret;
		}
		if (*ret == 0) {
			ret = nullptr;
		}
	}
	strlwr(buffer);

	// TODO: joystick values
	if (strcmp(buffer, "lmb") == 0) {
		input.set_mouse(SDL_BUTTON_LEFT);
	}
	else if (strcmp(buffer, "rmb") == 0) {
		input.set_mouse(SDL_BUTTON_RIGHT);
	}
	else if (strcmp(buffer, "mmb") == 0) {
		input.set_mouse(SDL_BUTTON_MIDDLE);
	}
	else if (strcmp(buffer, "mb4") == 0) {
		input.set_mouse(SDL_BUTTON_X1);
	}
	else if (strcmp(buffer, "mb5") == 0) {
		input.set_mouse(SDL_BUTTON_X2);
	}
	else if (strcmp(buffer, "enter") == 0) {
		// common synonym - makes fewer users/players want to cry
		input.set_key(SDL_SCANCODE_RETURN);
	}
	else {
		SDL_Scancode key = SDL_GetScancodeFromName(buffer);
		if (key != SDL_SCANCODE_UNKNOWN) {
			input.set_key(key);
		}
		else {
			ERR("Unrecognized key name '%s'\n", buffer);
			input.clear();
		}
	}

	return ret;
}

static void bind_spec_pos(ControllerInstance* inst, int axis, const char* spec) {
	RealInput inp;
	for (int i = 0; i < 3; ++i) {
		spec = read_real_input(spec, inp);
		bind_axis(inst, axis, 1, inp, i);
	}
}

static void bind_spec_neg(ControllerInstance* inst, int axis, const char* spec) {
	RealInput inp;
	for (int i = 0; i < 3; ++i) {
		spec = read_real_input(spec, inp);
		bind_axis(inst, axis, -1, inp, i);
	}
}

static void bind_spec(ControllerInstance* inst, int button, const char* spec) {
	RealInput inp;
	for (int i = 0; i < 3; ++i) {
		spec = read_real_input(spec, inp);
		bind_button(inst, button, inp, i);
	}
}

void bind_from_ini(const char* controller, const char* input, const char* spec) {
	ControllerInstance* inst = get_controller_by_name(controller);
	if (inst != nullptr) {
		const VirtualController* type = inst->type;
		if (input[0] == '+' || input[0] == '-') {
			const char* const* axis = std::find_if(type->axis_names.begin(), type->axis_names.end(),
				[input](const char* axis) { return strcmp(input + 1, axis) == 0; });
			if (axis != type->axis_names.end()) {
				int axis_index = axis - type->axis_names.begin();
				if (input[0] == '+') {
					bind_spec_pos(inst, axis_index, spec);
				}
				else {
					bind_spec_neg(inst, axis_index, spec);
				}
			}
		}
		else {
			const char* const* button = std::find_if(type->button_names.begin(), type->button_names.end(),
				[input](const char* button) { return strcmp(input, button) == 0; });
			if (button != type->button_names.end()) {
				int button_index = button - type->button_names.begin();
				bind_spec(inst, button_index, spec);
			}
		}
	}
}

static bool get_spec(char* buffer, const binding_set& bindings) {
	buffer[0] = 0;
	bool notfirst = false;
	for (const auto& binding : bindings) {
		if (binding.type == RealInput::NONE) {
			continue;
		}
		if (notfirst) {
			strcat(buffer, ", ");
		}

		switch (binding.type) {
		case RealInput::KEYBOARD:
			strcat(buffer, SDL_GetScancodeName(binding.key));
			break;
		case RealInput::MOUSE:
			switch (binding.mbutton) {
			case SDL_BUTTON_LEFT:
				strcat(buffer, "lmb"); break;
			case SDL_BUTTON_RIGHT:
				strcat(buffer, "rmb"); break;
			case SDL_BUTTON_MIDDLE:
				strcat(buffer, "mmb"); break;
			default:
				{
					char tmp[16];
					sprintf(tmp, "mb%d", binding.mbutton);
					strcat(buffer, "lmb"); break;
				}
				break;
			}
			break;
		default:
			continue;
		}
		notfirst = true;
	}
	return notfirst;
}

void dump_controller_config(FILE* stream) {
	for (auto& entry : controllers) {
		fprintf(stream, "\n[Input_%s]\n", entry.name);

		char specbuf[120];

		auto* inst = entry.inst;
		auto* type = inst->type;
		int n_axes = inst->axes.size();
		for (int i = 0; i < n_axes; ++i) {
			if (get_spec(specbuf, inst->axes[i].bindings_positive)) {
				fprintf(stream, "+%s=%s\n", type->axis_names[i], specbuf);
			}
			if (get_spec(specbuf, inst->axes[i].bindings_negative)) {
				fprintf(stream, "-%s=%s\n", type->axis_names[i], specbuf);
			}
		}

		int n_btns = inst->buttons.size();
		for (int i = 0; i < n_btns; ++i) {
			if (get_spec(specbuf, inst->buttons[i].bindings)) {
				fprintf(stream, "%s=%s\n", type->button_names[i], specbuf);
			}
		}
	}
}

#pragma endregion

// === Script Interface ===
#pragma region ScriptAPI

static void SetOnPressCallback(VirtualButtonState* button, asIScriptFunction* callback) {
	button->on_press.set_delegate(callback);
}

static void SetOnReleaseCallback(VirtualButtonState* button, asIScriptFunction* callback) {
	button->on_release.set_delegate(callback);
}

static asITypeInfo* strarray_type;

void RegisterInputTypes(asIScriptEngine* engine) {
#define check(EXPR) do {int r = (EXPR); assert(r >= 0);} while(0)
	strarray_type = engine->GetTypeInfoByDecl("array<string>");

	check(engine->SetDefaultNamespace("Input"));

	check(engine->RegisterObjectType("Axis", 0, asOBJ_REF | asOBJ_NOCOUNT));

	check(engine->RegisterObjectProperty("Axis", "const float position", asOFFSET(VirtualAxisState, position)));
	check(engine->RegisterObjectProperty("Axis", "const float velocity", asOFFSET(VirtualAxisState, velocity)));

	check(engine->RegisterObjectType("Button", 0, asOBJ_REF | asOBJ_NOCOUNT));

	check(engine->RegisterObjectProperty("Button", "const bool state", asOFFSET(VirtualButtonState, state)));
	check(engine->RegisterObjectProperty("Button", "const bool pressed", asOFFSET(VirtualButtonState, pressed)));
	check(engine->RegisterObjectProperty("Button", "const bool released", asOFFSET(VirtualButtonState, released)));

	check(engine->RegisterFuncdef("void ButtonEventCallback()"));

	check(engine->RegisterObjectMethod("Button", "void bind_on_press(ButtonEventCallback@)",
		asFUNCTION(SetOnPressCallback), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("Button", "void bind_on_release(ButtonEventCallback@)",
		asFUNCTION(SetOnReleaseCallback), asCALL_CDECL_OBJFIRST));

	check(engine->SetDefaultNamespace(""));
#undef check
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

#define check(EXPR) do {int r = (EXPR); if (r < 0) return r;} while(0)
static int RegisterControllerType(asIScriptEngine* engine, const VirtualController* vcont, const char* const name) {
	char declbuf[256];

	check(engine->RegisterObjectType(name, 0, asOBJ_REF | asOBJ_NOCOUNT));

	int n_axes = vcont->axis_names.size();
	for (int i = 0; i < n_axes; ++i) {
		sprintf(declbuf, "Axis %s", vcont->axis_names[i]);
		check(engine->RegisterObjectProperty(name, declbuf, GetAxisOffset(vcont, i)));
	}

	int n_buttons = vcont->button_names.size();
	for (int i = 0; i < n_buttons; ++i) {
		sprintf(declbuf, "Button %s", vcont->button_names[i]);
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

	check(engine->RegisterObjectMethod(name, "Axis@ axis(int)",
		asFUNCTION(GetAxisByIndex), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod(name, "Axis@ axis(const string &in)",
		asFUNCTION(GetAxisByName), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod(name, "Button@ button(int)",
		asFUNCTION(GetButtonByIndex), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod(name, "Button@ button(const string &in)",
		asFUNCTION(GetButtonByName), asCALL_CDECL_OBJFIRST));

	return 0;
}

int RegisterControllerTypes(asIScriptEngine* engine) {
	check(engine->SetDefaultNamespace("Input"));
	for (TypeEntry& entry : cont_types) {
		check(RegisterControllerType(engine, entry.type, entry.name));
	}

	char declbuf[256];

	check(engine->SetDefaultNamespace("Controllers"));
	for (auto& entry : controllers) {
		sprintf(declbuf, "Input::%s %s", entry.inst->type->name, entry.name);
		check(engine->RegisterGlobalProperty(declbuf, entry.inst));
	}
	check(engine->SetDefaultNamespace(""));

	return 0;
}
#undef check

#pragma endregion