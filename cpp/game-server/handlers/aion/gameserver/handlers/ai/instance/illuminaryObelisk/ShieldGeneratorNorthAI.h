#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorAI.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * The north shield generator of the Illuminary Obelisk ("northern_generator").
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorNorthAI.java, @AIName("northern_generator") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class ShieldGeneratorNorthAI : public ShieldGeneratorAI {
public:
	explicit ShieldGeneratorNorthAI(Npc& owner) : ShieldGeneratorAI(owner) {}

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
