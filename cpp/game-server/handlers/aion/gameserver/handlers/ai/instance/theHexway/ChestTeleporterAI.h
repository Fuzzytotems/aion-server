#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

/**
 * The AI of the Hexway's chest teleporter ("chest_teleporter"): its use takes the player to the bonus chest; a living player who comes
 * within 8 m in its sight is told once about the orb hidden in the chest.
 * <p>
 * Java: data/handlers/ai/instance/theHexway/ChestTeleporterAI.java, @AIName("chest_teleporter").
 *
 * @author Sykra
 */
class ChestTeleporterAI : public ActionItemNpcAI {
private:
	runtime::ArrayList<int32_t> recievedShouts{AION_LOCK_CLASS(ChestTeleporterAI::recievedShouts)}; // Java: = new ArrayList<>()

public:
	explicit ChestTeleporterAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

	void handleCreatureMoved(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::theHexway
