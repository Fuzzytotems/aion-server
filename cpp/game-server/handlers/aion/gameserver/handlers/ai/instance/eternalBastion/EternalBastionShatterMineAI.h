#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionConstructAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of the shatter mines ("eternal_bastion_shatter_mine"): searching, they blow up a player transformed into the tank (284320).
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionShatterMineAI.java, @AIName("eternal_bastion_shatter_mine")
 * (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionShatterMineAI : public EternalBastionConstructAI {
public:
	explicit EternalBastionShatterMineAI(Npc& owner) : EternalBastionConstructAI(owner) {}

protected:
	void handleSpawned() override;

	void handleCreatureSee(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
