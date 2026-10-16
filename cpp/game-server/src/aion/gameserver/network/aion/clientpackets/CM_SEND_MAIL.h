#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player sends a letter (C_MAIL_WRITE): recipient, title, message, an attached item and count, attached kinah and the letter type (normal or
 * express).
 * <p>
 * C++ only: `CM_SEND_MAILTestAccess` (tests/cm_lz/MailPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author Aion Gates, xTz
 */
class CM_SEND_MAIL : public AionClientPacket {
	friend struct CM_SEND_MAILTestAccess;

private:
	std::string recipientName;
	std::string title;
	std::string message;
	int32_t itemObjId{};
	int64_t itemCount{};
	int64_t kinahCount{};
	int32_t idLetterType{};

public:
	CM_SEND_MAIL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
