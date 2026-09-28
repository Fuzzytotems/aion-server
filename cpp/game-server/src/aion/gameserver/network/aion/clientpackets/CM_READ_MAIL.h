#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player opens a letter of his mailbox (C_MAIL_READ).
 * <p>
 * C++ only: `CM_READ_MAILTestAccess` (tests/cm_lz/MailPacketsTest.cpp) reads the field readImpl decoded, which Java keeps package-private
 * without a getter.
 *
 * @author kosyachok
 */
class CM_READ_MAIL : public AionClientPacket {
	friend struct CM_READ_MAILTestAccess;

private:
	int32_t mailObjId{};

public:
	CM_READ_MAIL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
