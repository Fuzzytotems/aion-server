#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player takes a letter's attachment (C_MAIL_GETITEM): its item (type 0) or its kinah (type 1).
 * <p>
 * C++ only: `CM_GET_MAIL_ATTACHMENTTestAccess` (tests/cm_ak/MailPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without getters.
 *
 * @author kosyachok
 */
class CM_GET_MAIL_ATTACHMENT : public AionClientPacket {
	friend struct CM_GET_MAIL_ATTACHMENTTestAccess;

private:
	int32_t mailObjId{};
	int8_t attachmentType{};

public:
	CM_GET_MAIL_ATTACHMENT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
