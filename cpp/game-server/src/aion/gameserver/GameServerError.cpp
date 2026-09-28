#include "aion/gameserver/GameServerError.h"

namespace aion::gameserver {

namespace {

/** Java Throwable(Throwable cause): the detail message is cause.toString() (class name and message), null for a null cause */
std::string describe(const std::exception_ptr& cause) {
	if (!cause)
		return "";
	try {
		std::rethrow_exception(cause);
	} catch (const std::exception& e) {
		return e.what();
	} catch (...) {
		return "unknown exception";
	}
}

} // namespace

GameServerError::GameServerError() : Exception("") {
}

GameServerError::GameServerError(std::exception_ptr cause) : Exception(describe(cause), cause) {
}

GameServerError::GameServerError(const std::string& message) : Exception(message) {
}

GameServerError::GameServerError(const std::string& message, std::exception_ptr cause) : Exception(message, cause) {
}

} // namespace aion::gameserver
