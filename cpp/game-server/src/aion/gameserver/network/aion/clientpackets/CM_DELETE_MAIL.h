#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player deletes letters of his mailbox (C_MAIL_DELETE): an unsigned short count, then per letter its object id and an unknown byte.
 * <p>
 * C++ only: `CM_DELETE_MAILTestAccess` (tests/cm_ak/MailPacketsTest.cpp) reads the ids readImpl decoded, which Java keeps private without a
 * getter.
 *
 * @author kosyachok
 */
class CM_DELETE_MAIL : public AionClientPacket {
	friend struct CM_DELETE_MAILTestAccess;

private:
	std::vector<int32_t> mailObjIds;

public:
	CM_DELETE_MAIL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
