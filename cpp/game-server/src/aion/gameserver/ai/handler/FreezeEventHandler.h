#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the FREEZE and UNFREEZE general events: sets or clears the FREEZE sub state and lets the AI think.
 * <p>
 * A static-only utility class (hub-headers.md §11.1); the AI parameter is the erased `AbstractAI<? extends Creature>` (§8.1).
 *
 * @author Rolandas, Neon
 */
class FreezeEventHandler {
public:
	FreezeEventHandler() = delete;

	static void onUnfreeze(AbstractAI& ai);

	static void onFreeze(AbstractAI& ai);
};

} // namespace aion::gameserver::ai::handler
