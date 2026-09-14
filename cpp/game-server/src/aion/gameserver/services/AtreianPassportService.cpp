#include "aion/gameserver/services/AtreianPassportService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::services {

// callback at AtreianPassportService.java:47 (fieldmap key AtreianPassportService@L47:50)
AtreianPassportService::AtreianPassportService() : expireDate(calculatePassportExpireDate()) {
	AION_UNPORTED();
}

AtreianPassportService::~AtreianPassportService() = default;

AtreianPassportService& AtreianPassportService::getInstance() {
	static AtreianPassportService instance; // Java SingletonHolder
	return instance;
}

bool AtreianPassportService::isAtreianPassportDisabled() {
	AION_UNPORTED();
}

bool AtreianPassportService::isAtreianPassportDisabled(std::chrono::local_time<std::chrono::milliseconds> checkDateTime) {
	AION_UNPORTED();
}

std::optional<std::chrono::local_time<std::chrono::milliseconds>> AtreianPassportService::findLastRewardTime() {
	AION_UNPORTED();
}

std::optional<std::chrono::local_time<std::chrono::milliseconds>> AtreianPassportService::calculatePassportExpireDate() {
	AION_UNPORTED();
}

void AtreianPassportService::takeReward(model::gameobjects::player::Player& player, const std::unordered_map<int32_t, std::unordered_set<int32_t>>& passports) {
	AION_UNPORTED();
}

void AtreianPassportService::onLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AtreianPassportService::sendPassport(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AtreianPassportService::checkOnlineDate(model::account::Account& pa, std::chrono::local_time<std::chrono::milliseconds> now) {
	AION_UNPORTED();
}

commons::database::Date AtreianPassportService::getAttendDay(std::chrono::local_time<std::chrono::milliseconds> serverTime) {
	AION_UNPORTED();
}

void AtreianPassportService::checkPassportLimit(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AtreianPassportService::purgeExpiredPassports(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t AtreianPassportService::getAccountAgeInMonths(model::gameobjects::player::Player& player, commons::database::Date now) {
	AION_UNPORTED();
}

commons::database::Timestamp AtreianPassportService::nowTs() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
