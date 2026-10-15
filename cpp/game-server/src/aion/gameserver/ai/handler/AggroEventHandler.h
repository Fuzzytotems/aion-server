#pragma once

#include <cstdint>

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Turns an aggro decision into hate: the CREATURE_AGGRO event and the two "a friend needs support" events each schedule an AggroNotifier
 * 500 ms later, which adds one point of hate.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class AggroEventHandler {
private:
	/** Java: private static final class AggroNotifier implements Runnable (used only by the bodies, defined in the .cpp, §9.3) */
	class AggroNotifier;

	static constexpr int32_t SUPPORT_RANGE_OFFSET = 2; // might be <pushed_range> in client data

public:
	AggroEventHandler() = delete;

	static void onAggro(NpcAI& npcAI, model::gameobjects::Creature& target);

	static bool onCreatureNeedsSupport(NpcAI& npcAI, model::gameobjects::Creature& creatureAskingForSupport);

	static bool onCreatureNeedsSupportByGuard(NpcAI& npcAI, model::gameobjects::Creature& creatureAskingForSupport);

private:
	static bool isInSupportRange(model::gameobjects::Npc& npc, model::gameobjects::Creature& creatureAskingForSupport,
		model::gameobjects::Creature& target);
};

} // namespace aion::gameserver::ai::handler
