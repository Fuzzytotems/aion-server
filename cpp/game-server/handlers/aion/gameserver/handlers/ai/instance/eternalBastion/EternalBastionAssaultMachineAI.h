#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionConstructAI.h"

#include <string_view>

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's assault pods and siege towers ("eternal_bastion_assault_machine"): a skill area at the spawn, and three seconds
 * later its walking squad.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionAssaultMachineAI.java, @AIName("eternal_bastion_assault_machine")
 * (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class EternalBastionAssaultMachineAI : public EternalBastionConstructAI {
public:
	explicit EternalBastionAssaultMachineAI(Npc& owner) : EternalBastionConstructAI(owner) {}

	void handleSpawned() override;

private:
	void spawnSkillArea();

	void spawnSummons();

	void spawnWithWalker(int32_t npcId, float x, float y, float z, int8_t h, std::string_view walker);
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
