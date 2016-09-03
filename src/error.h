#pragma once

typedef struct error {
	int code;
	const char* description;
	const char* source;
} Error;

#define ERROR(code, desc) Error{code,desc,"##__FILE__, line ##__LINE__ in function ##__func__"}