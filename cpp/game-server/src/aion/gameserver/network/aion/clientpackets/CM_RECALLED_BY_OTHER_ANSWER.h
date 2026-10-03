#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Answer to SM_RECALLED_BY_OTHER (C_RECALLED_BY_OTHER_ANSWER).
 *
 * @author SVDNESS
 */
class CM_RECALLED_BY_OTHER_ANSWER : public AionClientPacket {
private:
	int32_t answer{};

public:
	CM_RECALLED_BY_OTHER_ANSWER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
