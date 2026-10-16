#include "aion/loginserver/controller/BannedIpController.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/loginserver/dao/BannedIpDAO.h"

namespace aion::loginserver::controller::BannedIpController {

using model::BannedIP;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.controller.BannedIpController"));
	return *logger;
}

struct State {
	std::mutex mutex;
	/** List of banned ip adresses */
	std::unordered_set<BannedIP> banList;
};

State& state() {
	static auto* s = new State();
	return *s;
}

void clean() {
	dao::BannedIpDAO::cleanExpiredBans();
}

} // namespace

void start() {
	clean();
	load();
}

void load() {
	reload();
}

void reload() {
	// we are not going to make ip ban every minute, so it's ok to simplify a concurrent code a bit
	std::unordered_set<BannedIP> bans = dao::BannedIpDAO::getAllBans();
	size_t size = bans.size();
	{
		std::lock_guard lock(state().mutex);
		state().banList.swap(bans);
	}
	log().info("BannedIpController loaded " + std::to_string(size) + " IP bans.");
}

bool isBanned(std::string_view ip) {
	std::lock_guard lock(state().mutex);
	for (const BannedIP& ipBan : state().banList) {
		if (ipBan.isActive() && commons::utils::NetworkUtils::checkIPMatching(ipBan.getMask(), ip))
			return true;
	}
	return false;
}

bool banIp(std::string_view ip) {
	return banIp(ip, std::nullopt);
}

bool banIp(std::string_view ip, std::optional<commons::database::Timestamp> expireTime) {
	BannedIP ipBan;
	ipBan.setMask(std::string(ip));
	ipBan.setTimeEnd(expireTime);
	{
		std::lock_guard lock(state().mutex);
		if (!state().banList.insert(ipBan).second)
			return false;
	}
	return dao::BannedIpDAO::insert(ipBan);
}

bool addOrUpdateBan(const BannedIP& ipBan) {
	if (!ipBan.getId()) {
		if (dao::BannedIpDAO::insert(ipBan)) {
			std::lock_guard lock(state().mutex);
			state().banList.insert(ipBan);
			return true;
		}
		return false;
	}
	return dao::BannedIpDAO::update(ipBan);
}

bool unbanIp(std::string_view ip) {
	std::optional<BannedIP> ipBan;
	{
		std::lock_guard lock(state().mutex);
		for (const BannedIP& ban : state().banList) {
			if (ban.getMask() == ip) {
				ipBan = ban;
				break;
			}
		}
	}
	if (ipBan && dao::BannedIpDAO::remove(*ipBan)) {
		std::lock_guard lock(state().mutex);
		state().banList.erase(*ipBan);
		return true;
	}
	return false;
}

} // namespace aion::loginserver::controller::BannedIpController
