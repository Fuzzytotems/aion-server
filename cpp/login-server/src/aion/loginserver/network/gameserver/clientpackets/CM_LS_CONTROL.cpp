#include "aion/loginserver/network/gameserver/clientpackets/CM_LS_CONTROL.h"

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/dao/AccountDAO.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_LS_CONTROL_RESPONSE.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_LS_CONTROL::readImpl() {
	type = readC();
	param = readC();
	accountId = readD();
	adminId = readD();
}

void CM_LS_CONTROL::runImpl() {
	std::shared_ptr<model::Account> account = dao::AccountDAO::getAccount(accountId);
	if (!account)
		throw commons::utils::IllegalStateException("Account " + std::to_string(accountId) + " does not exist");
	switch (type) {
		case 1:
			account->setAccessLevel(param);
			break;
		case 2:
			account->setMembership(param);
			break;
	}
	bool result = dao::AccountDAO::updateAccount(*account);
	sendPacket(std::make_shared<serverpackets::SM_LS_CONTROL_RESPONSE>(type, param, account->getId().value(), adminId, result));
}

} // namespace aion::loginserver::network::gameserver::clientpackets
