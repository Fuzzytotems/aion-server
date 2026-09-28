#pragma once

#include "aion/gameserver/ai/manager/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::manager {

/**
 * Broadcasts the emotions an NPC shows when it starts or stops attacking, following, walking, returning or idling.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). Java declares the methods `public static final`, which C++ spells `static`.
 *
 * @author ATracer
 */
class EmoteManager {
public:
	EmoteManager() = delete;

	/**
	 * Npc starts attacking from idle state
	 *
	 * @param owner
	 */
	static void emoteStartAttacking(model::gameobjects::Npc& owner, model::gameobjects::Creature& target);

	/**
	 * Npc stops attacking
	 *
	 * @param owner
	 */
	static void emoteStopAttacking(model::gameobjects::Npc& owner);

	/**
	 * Npc starts following other creature
	 *
	 * @param owner
	 */
	static void emoteStartFollowing(model::gameobjects::Npc& owner);

	/**
	 * Npc starts walking (either random or path)
	 *
	 * @param owner
	 */
	static void emoteStartWalking(model::gameobjects::Npc& owner);

	/**
	 * Npc stops walking
	 *
	 * @param owner
	 */
	static void emoteStopWalking(model::gameobjects::Npc& owner);

	/**
	 * Npc starts returning to spawn location
	 *
	 * @param owner
	 */
	static void emoteStartReturning(model::gameobjects::Npc& owner);

	/**
	 * Npc starts idling
	 *
	 * @param owner
	 */
	static void emoteStartIdling(model::gameobjects::Npc& owner);
};

} // namespace aion::gameserver::ai::manager
