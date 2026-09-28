#include "aion/loginserver/controller/BannedMacManager.h"

#include <chrono>

#include "aion/loginserver/dao/BannedMacDAO.h"

namespace aion::loginserver::controller {

using model::base::BannedMacEntry;

BannedMacManager& BannedMacManager::getInstance() {
	static auto* manager = new BannedMacManager(); // leaked: may be used until the process ends
	return *manager;
}

BannedMacManager::BannedMacManager() : bannedList(dao::BannedMacDAO::load()) {}

void BannedMacManager::unban(std::string_view address, std::string_view details) {
	{
		std::lock_guard lock(mutex);
		auto it = bannedList.find(std::string(address));
		if (it == bannedList.end())
			return;
		bannedList.erase(it);
	}
	dao::BannedMacDAO::remove(address);
}

void BannedMacManager::ban(std::string_view address, int64_t time, std::string_view details) {
	BannedMacEntry mac{std::string(address), commons::database::Timestamp{std::chrono::milliseconds(time)}, std::string(details)};
	{
		std::lock_guard lock(mutex);
		bannedList.insert_or_assign(std::string(address), mac);
	}
	dao::BannedMacDAO::update(mac);
}

std::unordered_map<std::string, BannedMacEntry> BannedMacManager::getMap() const {
	std::lock_guard lock(mutex);
	return bannedList;
}

void BannedMacManager::reload() {
	auto loaded = dao::BannedMacDAO::load();
	std::lock_guard lock(mutex);
	bannedList.swap(loaded);
}

} // namespace aion::loginserver::controller
