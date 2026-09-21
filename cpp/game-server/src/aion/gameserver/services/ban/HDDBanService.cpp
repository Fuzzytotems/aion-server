#include "aion/gameserver/services/ban/HDDBanService.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_HDDBAN_CONTROL.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/ban/BanAction.h"

namespace aion::gameserver::services::ban {

HDDBanService::HDDBanService() = default;

HDDBanService::~HDDBanService() = default;

HDDBanService& HDDBanService::getInstance() {
	static HDDBanService instance; // Java SingletonHolder
	return instance;
}

void HDDBanService::addBan(std::string_view serial, std::optional<commons::database::Timestamp> banTime) {
	// Java stores a null banTime into the map and then throws NullPointerException at banTime.getTime(); the map cannot hold null, so the C++
	// port throws before storing (isBanned would throw for that serial in Java)
	if (!banTime)
		throw runtime::NullPointerException("banTime");
	bannedSerials.put(std::string(serial), *banTime);
	network::loginserver::LoginServer::getInstance().sendPacket(
		network::loginserver::serverpackets::SM_HDDBAN_CONTROL(BanAction::BAN, serial, banTime->time_since_epoch().count()));
}

void HDDBanService::removeBan(std::string_view serial) {
	this->bannedSerials.remove(std::string(serial));
	network::loginserver::LoginServer::getInstance().sendPacket(network::loginserver::serverpackets::SM_HDDBAN_CONTROL(BanAction::UNBAN, serial, 0));
}

void HDDBanService::loadBan(std::string_view serial, int64_t banTime) {
	this->bannedSerials.put(std::string(serial), commons::database::Timestamp(std::chrono::milliseconds(banTime)));
}

bool HDDBanService::isBanned(std::string_view serial) {
	std::string key(serial);
	if (!this->bannedSerials.containsKey(key))
		return false;
	std::optional<commons::database::Timestamp> banTime = bannedSerials.get(key);
	// java-race: containsKey and get are separate reads; a concurrent removeBan between them makes Java throw NullPointerException, kept here
	if (!banTime)
		throw runtime::NullPointerException("banTime");
	return banTime->time_since_epoch().count() > commons::utils::currentTimeMillis();
}

} // namespace aion::gameserver::services::ban
