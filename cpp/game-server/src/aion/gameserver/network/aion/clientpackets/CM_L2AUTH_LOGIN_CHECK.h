#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is authenticating himself by providing accountId and rest of sessionKey - we will check if its valid at login server
 * side.
 *
 * @author -Nemesiss-
 */
class CM_L2AUTH_LOGIN_CHECK : public AionClientPacket {
private:
	/** playOk2 is part of session key - its used for security purposes we will check if this is the key what login server sends. */
	int32_t playOk2{};
	/** playOk1 is part of session key - its used for security purposes we will check if this is the key what login server sends. */
	int32_t playOk1{};
	/**
	 * accountId is part of session key - its used for authentication we will check if this accountId is matching any waiting account login server
	 * side and check if rest of session key is ok.
	 */
	int32_t accountId{};
	/** loginOk is part of session key - its used for security purposes we will check if this is the key what login server sends. */
	int32_t loginOk{};
	/** The value the client got in bytes 16-23 of S_RESULT_PASSPORT (SM_UNK_3_5_1), 0 on a normal login. */
	int64_t passportKey{}; // Java: @SuppressWarnings("unused")

public:
	/** Constructs new instance of <tt>CM_L2AUTH_LOGIN_CHECK </tt> packet */
	CM_L2AUTH_LOGIN_CHECK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
