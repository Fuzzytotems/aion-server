#pragma once

#include <exception>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.SiegeException (a RuntimeException), thrown by Siege.initSiegeBoss and the siege subclasses. The four Java constructors:
 * (), (message), (message, cause), (cause) - the cause as std::current_exception() of a catch block.
 *
 * @author SoulKeeper
 */
class SiegeException : public runtime::Exception {
public:
	SiegeException() : runtime::Exception("") {}

	explicit SiegeException(const std::string& message) : runtime::Exception(message) {}

	SiegeException(const std::string& message, std::exception_ptr cause) : runtime::Exception(message, cause) {}

	/** Java SiegeException(Throwable cause): RuntimeException(cause) takes the cause's toString() as its message */
	explicit SiegeException(std::exception_ptr cause) : runtime::Exception(describe(cause), cause) {}

private:
	static std::string describe(std::exception_ptr cause) {
		if (!cause)
			return "";
		try {
			std::rethrow_exception(cause);
		} catch (const std::exception& e) {
			return e.what();
		} catch (...) {
			return "";
		}
	}
};

} // namespace aion::gameserver::services::siege
