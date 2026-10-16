#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_CONNECTION_INFO.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/dao/AccountDAO.h"
#include "aion/loginserver/dao/AccountsLogDAO.h"

namespace aion::loginserver::network::gameserver::clientpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_CONNECTION_INFO"));
	return *logger;
}

} // namespace

void CM_ACCOUNT_CONNECTION_INFO::readImpl() {
	accountId = readD();
	time = readQ();
	ip = readS();
	mac = readS();
	hddSerial = readS();
}

void CM_ACCOUNT_CONNECTION_INFO::runImpl() {
	if (!dao::AccountDAO::updateLastMac(accountId, mac))
		log().warn("Couldn't update account_data.last_mac for accountId " + std::to_string(accountId));
	if (!dao::AccountDAO::updateLastHDDSerial(accountId, hddSerial))
		log().warn("Couldn't update account_data.last_hdd_serial for accountId " + std::to_string(accountId));
	if (configs::Config::LOG_LOGINS)
		dao::AccountsLogDAO::addRecord(accountId, getGameServerInfo()->getId(), time, ip, mac, hddSerial);
}

} // namespace aion::loginserver::network::gameserver::clientpackets
