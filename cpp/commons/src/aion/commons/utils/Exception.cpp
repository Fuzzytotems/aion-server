#include "aion/commons/utils/Exception.h"

#include <typeinfo>

namespace aion::commons::utils {

Exception::Exception(const std::string& message, std::stacktrace trace) : std::runtime_error(message), trace(std::move(trace)) {
}

Exception::Exception(const std::string& message, std::exception_ptr cause, std::stacktrace trace)
	: std::runtime_error(message), trace(std::move(trace)), causePtr(std::move(cause)) {
}

std::string exceptionTypeName(const std::exception& e) {
	std::string name = typeid(e).name();
	for (std::string_view prefix : {"class ", "struct "}) {
		if (name.starts_with(prefix))
			name.erase(0, prefix.size());
	}
	return name;
}

namespace {

void appendException(std::string& out, const std::exception& e, int depth) {
	if (depth > 0)
		out += "\nCaused by: ";
	out += exceptionTypeName(e);
	std::string_view message = e.what();
	if (!message.empty()) {
		out += ": ";
		out += message;
	}
	if (depth >= 16) // guard against cyclic cause chains
		return;
	if (auto* ex = dynamic_cast<const Exception*>(&e)) {
		for (const auto& frame : ex->stacktrace()) {
			out += "\n\tat ";
			out += std::to_string(frame);
		}
		if (ex->cause()) {
			try {
				std::rethrow_exception(ex->cause());
			} catch (const std::exception& cause) {
				appendException(out, cause, depth + 1);
			} catch (...) {
				out += "\nCaused by: <unknown exception type>";
			}
		}
	}
}

} // namespace

std::string toStackTraceString(const std::exception& e) {
	std::string out;
	appendException(out, e, 0);
	return out;
}

} // namespace aion::commons::utils
