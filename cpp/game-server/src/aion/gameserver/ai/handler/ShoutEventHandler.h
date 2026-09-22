#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/npcshout/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Picks the shout an NPC says for each event and hands it to the NpcShoutsService.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). Every method starts with `npcAI.ask(AIQuestion::CAN_SHOUT)`, which is
 * `AIConfig::SHOUTS_ENABLE && NpcShoutsService::mayShout(owner)` (NpcAI.java:149) and therefore false while
 * `gameserver.npcshouts.enable` is off (its default, m5b-plan.md D1): the unported NpcShoutsService bodies are then never reached.
 *
 * @author Rolandas, Neon
 */
class ShoutEventHandler {
public:
	ShoutEventHandler() = delete;

	static void onSee(NpcAI& npcAI, model::gameobjects::Creature& target);

	static void onSpawn(NpcAI& npcAI);

	static void onBeforeDespawn(NpcAI& npcAI);

	static void onReachedWalkPoint(NpcAI& npcAI);

	static void onSwitchedTarget(NpcAI& npcAI, model::gameobjects::Creature& target);

	static void onDied(NpcAI& npcAI);

	/**
	 * Called on Aggro when NPC is ready to attack
	 */
	static void onAttackBegin(NpcAI& npcAI);

	/**
	 * Handle NPC attacked event (when damage was received or not)
	 */
	static void onEnemyAttack(NpcAI& npcAI, model::gameobjects::Creature& attacker);

	static void onCast(NpcAI& npcAI, model::gameobjects::Creature& firstTarget);

	/**
	 * Handle target attacked events
	 */
	static void onAttack(NpcAI& npcAI, model::gameobjects::Creature& attacked);

private:
	static void handleNumericEvent(NpcAI& npcAI, model::gameobjects::player::Player& creature,
		model::templates::npcshout::ShoutEventType eventType);

public:
	static void onAttackEnd(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
