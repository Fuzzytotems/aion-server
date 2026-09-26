#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is asking if may login into game [ie start playing].
 *
 * @author -Nemesiss-
 */
class CM_MAY_LOGIN_INTO_GAME : public AionClientPacket {
public:
	/** Constructs new instance of <tt>CM_MAY_LOGIN_INTO_GAME </tt> packet */
	CM_MAY_LOGIN_INTO_GAME(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
