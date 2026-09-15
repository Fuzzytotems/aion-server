#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services::cron {
class JobDetail;
} // namespace aion::gameserver::services::cron

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * LocalDateTime is std::chrono::local_time<std::chrono::milliseconds>, LocalDate commons::database::Date (hub-headers.md §6); the cron job
 * handle is services::cron::JobDetail.
 *
 * @author ViAl, Luzien, SVDNESS
 */
class AtreianPassportService : public runtime::Immortal {
private:
	static constexpr std::string_view DAILY_CRON_AT_09_00 = "0 0 9 ? * *";
	static constexpr int32_t ATTEND_RESET_HOUR = 9;
	// fieldmap.toml: java.time.LocalDateTime (hub-headers.md §6), null when no passport has a period end
	const std::optional<std::chrono::local_time<std::chrono::milliseconds>> expireDate;
	runtime::Field<runtime::Ref<cron::JobDetail>> cronInfo{}; // fieldmap.toml: Quartz JobDetail is services::cron::JobDetail, a RefCounted handle (hub-headers.md §6)
	AtreianPassportService();
	~AtreianPassportService();
public:
	bool isAtreianPassportDisabled();
private:
	bool isAtreianPassportDisabled(std::chrono::local_time<std::chrono::milliseconds> checkDateTime);
	std::optional<std::chrono::local_time<std::chrono::milliseconds>> findLastRewardTime();
	std::optional<std::chrono::local_time<std::chrono::milliseconds>> calculatePassportExpireDate();
public:
	void takeReward(model::gameobjects::player::Player& player, const std::unordered_map<int32_t, std::unordered_set<int32_t>>& passports);
	void onLogin(model::gameobjects::player::Player& player);
private:
	void sendPassport(model::gameobjects::player::Player& player);
	bool checkOnlineDate(model::account::Account& pa, std::chrono::local_time<std::chrono::milliseconds> now);
	commons::database::Date getAttendDay(std::chrono::local_time<std::chrono::milliseconds> serverTime);
	void checkPassportLimit(model::gameobjects::player::Player& player);
	void purgeExpiredPassports(model::gameobjects::player::Player& player);
	/**
	 * Calculates the number of full months between the account creation date and the given date.
	 * The calculation is based on year and month difference. If the day of the given date is
	 * earlier than the day of the creation date, the current month is considered incomplete
	 * and is not counted.
	 * The returned value is always non-negative.
	 */
	int32_t getAccountAgeInMonths(model::gameobjects::player::Player& player, commons::database::Date now);
	static commons::database::Timestamp nowTs();
public:
	static AtreianPassportService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
