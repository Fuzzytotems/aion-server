#include "aion/loginserver/network/gameserver/clientpackets/CM_CHANGE_ALLOWED_HDD_SERIAL.h"

#include "aion/loginserver/dao/AccountDAO.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_CHANGE_ALLOWED_HDD_SERIAL::readImpl() {
	accountId = readD();
	hddSerial = readS();
}

void CM_CHANGE_ALLOWED_HDD_SERIAL::runImpl() {
	dao::AccountDAO::updateAllowedHDDSerial(accountId, hddSerial);
}

} // namespace aion::loginserver::network::gameserver::clientpackets
