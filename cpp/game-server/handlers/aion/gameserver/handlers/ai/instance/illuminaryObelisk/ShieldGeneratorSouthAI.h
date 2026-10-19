#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorAI.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * The south shield generator of the Illuminary Obelisk ("southern_generator").
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorSouthAI.java, @AIName("southern_generator") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class ShieldGeneratorSouthAI : public ShieldGeneratorAI {
public:
	explicit ShieldGeneratorSouthAI(Npc& owner) : ShieldGeneratorAI(owner) {}

protected:
	network::aion::serverpackets::SM_SYSTEM_MESSAGE getAttackMsg() override;

	network::aion::serverpackets::SM_SYSTEM_MESSAGE getChargeMsg() override;

	network::aion::serverpackets::SM_SYSTEM_MESSAGE getDestructionMsg() override;

	network::aion::serverpackets::SM_SYSTEM_MESSAGE getGateMsg() override;

	void handleChargeComplete() override;

	void handleVortexSpawn() override;

	void handleVortexDespawn() override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
