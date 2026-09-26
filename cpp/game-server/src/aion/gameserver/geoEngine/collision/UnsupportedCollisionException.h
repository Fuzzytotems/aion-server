#pragma once

#include <exception>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::geoEngine::collision {

/**
 * Thrown for collisions the engine does not implement.
 * <p>
 * K5 confined (fieldmap). C++: every collideWith of the port takes a math::Ray (Collidable.h), so the geo engine itself never throws it; it is
 * kept for callers that check the Java type. Java's constructors taking only a cause map to the (message, cause) form.
 *
 * @author Kirill
 */
class UnsupportedCollisionException : public runtime::UnsupportedOperationException {
public:
	UnsupportedCollisionException() : UnsupportedOperationException("") {}
	explicit UnsupportedCollisionException(const std::string& message) : UnsupportedOperationException(message) {}
	UnsupportedCollisionException(const std::string& message, std::exception_ptr cause) : UnsupportedOperationException(message, cause) {}
	explicit UnsupportedCollisionException(std::exception_ptr cause) : UnsupportedOperationException("", cause) {}
};

} // namespace aion::gameserver::geoEngine::collision
