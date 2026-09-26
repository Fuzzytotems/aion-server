#pragma once

#include <string_view>

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai {

/**
 * Logs AI debug messages ("[AI] <objectId> - <message>") for AIs with logging enabled (AIConfig.ONCREATE_DEBUG, `//ai log`).
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class AILogger {
public:
	AILogger() = delete;

	static void info(AbstractAI& ai, std::string_view message);

	/**
	 * @param owner
	 * @param message
	 */
	static void moveinfo(model::gameobjects::Creature& owner, std::string_view message);
};

} // namespace aion::gameserver::ai
