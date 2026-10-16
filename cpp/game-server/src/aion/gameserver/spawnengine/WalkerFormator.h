#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * Forms the walker groups on initial spawn<br>
 * Brings NPCs back to their positions if they die<br>
 * Cleanup and rework will be made after tests and error handling<br>
 * To use only with patch!
 * <p>
 * C++: a static-only class.
 *
 * @author vlog (based on Imaginary's imagination), Rolandas
 */
class WalkerFormator {
public:
	WalkerFormator() = delete;

	/**
	 * If it's the instance first spawn, WalkerFormator verifies and creates groups; {@link #organizeAndSpawn()} must be called after to speed up
	 * spawning. If it's a respawn, nothing to verify, then the method places NPC to the first step and resets data to the saved, no organizing is
	 * needed.
	 *
	 * @return <tt>true</tt> if npc was brought into world by the method call.
	 */
	static bool processClusteredNpc(model::gameobjects::Npc& npc, int32_t worldId, int32_t instanceId);

	/** Organizes spawns in all processed walker groups. Must be called only when spawning all npcs for the instance of world. */
	static void organizeAndSpawn(int32_t worldId, int32_t instanceId);

	static void changeWalkerGroup(int32_t worldId, int32_t instanceId, WalkerGroup& walkerGroup);

	static void onInstanceDestroy(int32_t worldId, int32_t instanceId);
};

} // namespace aion::gameserver::spawnengine
