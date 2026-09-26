#include "aion/loginserver/controller/BannedHDDController.h"

#include <chrono>

#include "aion/loginserver/dao/BannedHddDAO.h"

namespace aion::loginserver::controller {

using commons::database::Timestamp;

BannedHDDController& BannedHDDController::getInstance() {
	static auto* instance = new BannedHDDController(); // leaked: may be used until the process ends
	return *instance;
}

BannedHDDController::BannedHDDController() : bannedList(dao::BannedHddDAO::load()) {}

void BannedHDDController::unban(std::string_view serial) {
	{
		std::lock_guard lock(mutex);
		auto it = bannedList.find(std::string(serial));
		if (it == bannedList.end())
			return;
		bannedList.erase(it);
	}
	dao::BannedHddDAO::remove(serial);
}

void BannedHDDController::ban(std::string_view serial, int64_t time) {
	Timestamp banTime{std::chrono::milliseconds(time)};
	{
		std::lock_guard lock(mutex);
		bannedList.insert_or_assign(std::string(serial), banTime);
	}
	dao::BannedHddDAO::update(serial, banTime);
}

std::unordered_map<std::string, Timestamp> BannedHDDController::getMap() const {
	std::lock_guard lock(mutex);
	return bannedList;
}

void BannedHDDController::reload() {
	auto loaded = dao::BannedHddDAO::load();
	std::lock_guard lock(mutex);
	bannedList.swap(loaded);
}

} // namespace aion::loginserver::controller
