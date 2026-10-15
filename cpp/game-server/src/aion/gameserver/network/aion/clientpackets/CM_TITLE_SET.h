#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player chooses the title to display, or none (C_CHANGE_TITLE).
 *
 * @author Nemiroff, cura
 */
class CM_TITLE_SET : public AionClientPacket {
private:
	int32_t titleId{};

public:
	CM_TITLE_SET(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
