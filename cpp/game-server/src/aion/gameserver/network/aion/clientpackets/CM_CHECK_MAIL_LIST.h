#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client asks for the mailbox's letter list (C_MAIL_LIST), all letters or the express ones only.
 *
 * @author ginho1
 */
class CM_CHECK_MAIL_LIST : public AionClientPacket {
public:
	bool expressOnly{};

	CM_CHECK_MAIL_LIST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
