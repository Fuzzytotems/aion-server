#pragma once

#include <exception>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.BaseException (a RuntimeException), thrown by Base and BaseService. The four Java constructors:
 * (), (message), (message, cause), (cause) - the cause as std::current_exception() of a catch block.
 *
 * @author Estrayl
 */
class BaseException : public runtime::Exception {
public:
	BaseException() : runtime::Exception("") {}

	explicit BaseException(const std::string& message) : runtime::Exception(message) {}

	BaseException(const std::string& message, std::exception_ptr cause) : runtime::Exception(message, cause) {}

	/** Java BaseException(Throwable cause): RuntimeException(cause) takes the cause's toString() as its message */
	explicit BaseException(std::exception_ptr cause) : runtime::Exception(describe(cause), cause) {}

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

} // namespace aion::gameserver::model::base
