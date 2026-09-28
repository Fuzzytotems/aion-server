#pragma once

#include <exception>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/fwd.h"

namespace aion::gameserver {

/**
 * Superclass of GameServer errors.
 * <p>
 * C++: Java's Error subclass is a commons Exception (CONVENTIONS.md "Errors and exceptions": fatal startup errors are exceptions that main
 * logs). The constructor without a message uses an empty message (Java: null); the cause-only constructor uses the cause's description (Java:
 * cause.toString()).
 *
 * @author Aquanox
 */
class GameServerError : public commons::utils::Exception {
public:
	GameServerError();

	explicit GameServerError(std::exception_ptr cause);

	explicit GameServerError(const std::string& message);

	GameServerError(const std::string& message, std::exception_ptr cause);
};

} // namespace aion::gameserver
