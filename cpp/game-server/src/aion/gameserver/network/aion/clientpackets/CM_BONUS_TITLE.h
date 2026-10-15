#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player chooses which owned title gives its stat bonus, or none (C_CHANGE_ATTR_TITLE).
 *
 * @author -Enomine-
 */
class CM_BONUS_TITLE : public AionClientPacket {
private:
	int32_t bonusTitleId{};

public:
	CM_BONUS_TITLE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
