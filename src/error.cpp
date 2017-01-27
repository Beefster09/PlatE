
#include "error.h"

std::string std::to_string(const Error& err) {
	std::string str("Error #");
	str += std::to_string(err.code);
	str += ": ";
	str += err.description;
	str += " (";
	str += err.details;
	str += ')';
	return str;
}

void DispatchErrorCallback(asIScriptFunction* callback, const Error& err) {
	std::string desc(err.description);
	desc += ": ";
	desc += err.details;

	asIScriptEngine* engine = callback->GetEngine();
	asIScriptContext* ctx = engine->RequestContext();

	ctx->Prepare(callback);
	ctx->SetArgDWord(0, err.code);
	ctx->SetArgObject(1, &desc);

	if (ctx->Execute() != asEXECUTION_FINISHED) {
		ERR_DEBUG("Error callback did not return.\n");
	}
	ctx->Unprepare();
	engine->ReturnContext(ctx);
}

std::string GetExceptionDetails(asIScriptContext* ctx) {
	std::string details = ctx->GetExceptionString();
	details += " (";
	details += ctx->GetExceptionFunction()->GetName();
	details += ", line #";
	details += std::to_string(ctx->GetExceptionLineNumber());
	details += ')';
	return details;
}