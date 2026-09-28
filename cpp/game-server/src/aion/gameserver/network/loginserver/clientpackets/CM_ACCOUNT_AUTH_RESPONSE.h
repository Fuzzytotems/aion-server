#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * In this packet LoginServer is answering on GameServer request about valid authentication data and also sends account name of user that is
 * authenticating on GameServer.
 *
 * @author -Nemesiss-
 */
class CM_ACCOUNT_AUTH_RESPONSE : public LsClientPacket {
public:
	explicit CM_ACCOUNT_AUTH_RESPONSE(int32_t opCode);
	~CM_ACCOUNT_AUTH_RESPONSE() override;

private:
	int32_t accountId = 0;
	/** result - true = authed */
	bool result = false;
	/** accountName [if response is ok] */
	std::string accountName;
	/** Time of account creation, measured in milliseconds since 1.1.1970 0:00 UTC */
	int64_t creationDate = 0;
	/** null if the result is false */
	runtime::Ref<model::account::AccountTime> accountTime;
	/** access level - regular/gm/admin */
	int8_t accessLevel = 0;
	/** Membership - regular/premium */
	int8_t membership = 0;
	std::string allowedHddSerial;

public:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
