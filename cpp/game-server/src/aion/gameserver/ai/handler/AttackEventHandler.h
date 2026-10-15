#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Starts, paces and ends the fight of an NPC: the ATTACK creature event puts it into AIState::FIGHT and hands it to the AttackManager, and
 * ATTACK_COMPLETE / ATTACK_FINISH drive the next swing and the return to idle.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). `onAttack`'s creature is a `Ptr` because Java checks it for null
 * (AttackEventHandler.java:28) and onForcedAttack casts a possibly-null target into it.
 *
 * @author ATracer
 */
class AttackEventHandler {
public:
	AttackEventHandler() = delete;

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void onAttack(NpcAI& npcAI, runtime::Ptr<model::gameobjects::Creature> creature);

	/**
	 * @param npcAI
	 */
	static void onForcedAttack(NpcAI& npcAI);

	/**
	 * @param npcAI
	 */
	static void onAttackComplete(NpcAI& npcAI);

	/**
	 * @param npcAI
	 */
	static void onFinishAttack(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
