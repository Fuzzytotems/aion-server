#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the MOVE_VALIDATE and MOVE_ARRIVED general events of an NPC.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). Java declares both methods `public static final`, which C++ spells `static`.
 *
 * @author ATracer
 */
class MoveEventHandler {
public:
	MoveEventHandler() = delete;

	/**
	 * @param npcAI
	 */
	static void onMoveValidate(NpcAI& npcAI);

	/**
	 * @param npcAI
	 */
	static void onMoveArrived(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
