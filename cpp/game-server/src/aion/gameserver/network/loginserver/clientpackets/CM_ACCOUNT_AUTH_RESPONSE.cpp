#include "aion/gameserver/network/loginserver/clientpackets/CM_ACCOUNT_AUTH_RESPONSE.h"

#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_ACCOUNT_AUTH_RESPONSE::CM_ACCOUNT_AUTH_RESPONSE(int32_t opCode) : LsClientPacket(opCode) {
}

CM_ACCOUNT_AUTH_RESPONSE::~CM_ACCOUNT_AUTH_RESPONSE() = default;

void CM_ACCOUNT_AUTH_RESPONSE::readImpl() {
	accountId = readD();
	result = readC() == 1;

	if (result) {
		accountName = readS();
		creationDate = readQ();
		accountTime = model::account::AccountTime::create();
		accountTime->setAccumulatedOnlineTime(readQ());
		accountTime->setAccumulatedRestTime(readQ());
		accessLevel = readC();
		membership = readC();
		allowedHddSerial = readS();
	}
}

void CM_ACCOUNT_AUTH_RESPONSE::runImpl() {
	LoginServer::getInstance().accountAuthenticationResponse(accountId, accountName, result, creationDate, accountTime, accessLevel, membership,
		allowedHddSerial);
}

} // namespace aion::gameserver::network::loginserver::clientpackets
