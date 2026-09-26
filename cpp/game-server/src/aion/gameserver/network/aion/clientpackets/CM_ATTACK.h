#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet for every melee auto-attack swing.
 *
 * @author alexa026, Avol, ATracer, KID
 */
class CM_ATTACK : public AionClientPacket {
private:
	/** Target object id that client wants to TALK WITH or 0 if wants to unselect */
	int32_t targetObjectId{};
	// TODO: Question, are they really needed?
	/** Java: @SuppressWarnings("unused") - read from the body and never used */
	int32_t attackno{};

	int32_t time{};
	/** Java: @SuppressWarnings("unused") - read from the body and never used */
	int32_t type{};

public:
	CM_ATTACK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
