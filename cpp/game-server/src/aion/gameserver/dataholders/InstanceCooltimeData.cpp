#include "aion/gameserver/dataholders/InstanceCooltimeData.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/instance/InstanceCoolTimeType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::dataholders {

using model::instance::InstanceCoolTimeType;
using model::templates::InstanceCooltime;

void InstanceCooltimeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const InstanceCooltime& tmp : instanceCooltime) {
		if (instanceCooltimes.insert_or_assign(tmp.getWorldId(), &tmp).second)
			worldIdsInOrder.push_back(tmp.getWorldId());
		syncIdToMapId.insert_or_assign(tmp.getSyncId(), tmp.getWorldId());
	}
	// Java: instanceCooltime.clear() (the C++ indexes point into the storage, which stays)
}

std::vector<std::pair<int32_t, const InstanceCooltime*>> InstanceCooltimeData::getInstanceCooltimes() const {
	std::vector<std::pair<int32_t, const InstanceCooltime*>> copy;
	copy.reserve(worldIdsInOrder.size());
	for (int32_t worldId : worldIdsInOrder)
		copy.emplace_back(worldId, instanceCooltimes.at(worldId));
	return copy;
}

const InstanceCooltime* InstanceCooltimeData::getInstanceCooltimeByWorldId(int32_t worldId) const {
	auto it = instanceCooltimes.find(worldId);
	return it != instanceCooltimes.end() ? it->second : nullptr;
}

int32_t InstanceCooltimeData::getInstanceMaxCountByWorldId(int32_t worldId) const {
	const InstanceCooltime* template_ = getInstanceCooltimeByWorldId(worldId);
	if (template_ == nullptr)
		throw runtime::NullPointerException(
		  "Cannot invoke \"InstanceCooltime.getMaxCount()\" because the return value of \"java.util.Map.get(Object)\" is null");
	return template_->getMaxCount();
}

int32_t InstanceCooltimeData::getMaxMemberCount(int32_t worldId, model::Race race) const {
	const InstanceCooltime* template_ = getInstanceCooltimeByWorldId(worldId);
	return template_ == nullptr ? 0 : race == model::Race::ELYOS ? template_->getMaxMemberLight() : template_->getMaxMemberDark();
}

int32_t InstanceCooltimeData::getWorldId(int32_t syncId) const {
	auto it = syncIdToMapId.find(syncId);
	return it != syncIdToMapId.end() ? it->second : 0;
}

int64_t InstanceCooltimeData::calculateInstanceEntranceCooltime(model::gameobjects::player::Player& player, int32_t worldId) const {
	namespace chrono = std::chrono;
	using utils::time::ServerTime;
	int32_t instanceCooldownRate = services::instance::InstanceService::getInstanceRate(player, worldId);
	int64_t instanceCoolTime = 0;
	const InstanceCooltime* clt = getInstanceCooltimeByWorldId(worldId);
	if (clt == nullptr || clt->getMaxCount() == 0)
		return 0;
	if (!clt->getCoolTimeType())
		throw runtime::NullPointerException("InstanceCooltime.getCoolTimeType() is null"); // Java: switch on a null enum
	switch (*clt->getCoolTimeType()) {
		case InstanceCoolTimeType::DAILY:
		case InstanceCoolTimeType::WEEKLY: {
			int32_t hour = clt->getEntCoolTime() / 100;
			int32_t minute = clt->getEntCoolTime() % 100;
			if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
				throw runtime::IllegalArgumentException("Invalid value for HourOfDay or MinuteOfHour: " + std::to_string(hour) + ":" +
				                                        std::to_string(minute));
			ServerTime::ZonedDateTime now = ServerTime::now();
			// Java: now.with(LocalTime.of(hour, minute))
			const chrono::local_days today = chrono::floor<chrono::days>(now.get_local_time());
			ServerTime::LocalDateTime repeatLocal = today + chrono::hours(hour) + chrono::minutes(minute);
			ServerTime::ZonedDateTime repeatDate = ServerTime::of(repeatLocal);
			if (now.get_sys_time() > repeatDate.get_sys_time()) { // Java: now.isAfter(repeatDate)
				repeatLocal += chrono::days(1);
				repeatDate = ServerTime::of(repeatLocal);
			}
			if (*clt->getCoolTimeType() == InstanceCoolTimeType::WEEKLY) {
				const chrono::weekday day{chrono::floor<chrono::days>(repeatDate.get_local_time())};
				repeatLocal += chrono::days(calculateDaysUntilReset(*clt, static_cast<int32_t>(day.iso_encoding())));
				repeatDate = ServerTime::of(repeatLocal);
			}
			// Java: repeatDate.toEpochSecond() * 1000
			instanceCoolTime = chrono::floor<chrono::seconds>(repeatDate.get_sys_time()).time_since_epoch().count() * 1000;
			break;
		}
		case InstanceCoolTimeType::RELATIVE: {
			int32_t minutes = clt->getEntCoolTime();
			if (minutes == 0) // unlimited entrance, no need to store
				return 0;
			// Java: minutes * 60 * 1000 is int arithmetic
			int32_t millis = static_cast<int32_t>(static_cast<uint32_t>(minutes) * 60u * 1000u);
			instanceCoolTime = commons::utils::currentTimeMillis() + millis;
			break;
		}
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.InstanceCooltimeData")
			  .warn("Unhandled InstanceCoolTimeType: " + std::string(xml::enumName(*clt->getCoolTimeType())));
	}
	if (instanceCooldownRate != 1) {
		if (instanceCooldownRate == 0)
			throw commons::utils::ArithmeticException("/ by zero");
		instanceCoolTime = commons::utils::currentTimeMillis() + ((instanceCoolTime - commons::utils::currentTimeMillis()) / instanceCooldownRate);
	}
	return instanceCoolTime;
}

int32_t InstanceCooltimeData::calculateDaysUntilReset(const InstanceCooltime& clt, int32_t dayOfWeek) {
	std::vector<int32_t> resetDaysSorted;
	for (const std::string& dayStr : commons::utils::StringUtils::splitJava(clt.getTypeValue(), ","))
		resetDaysSorted.push_back(getDay(dayStr));
	std::sort(resetDaysSorted.begin(), resetDaysSorted.end());
	for (int32_t resetDay : resetDaysSorted) {
		if (resetDay >= dayOfWeek)
			return resetDay - dayOfWeek;
	}
	if (resetDaysSorted.empty())
		throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return (7 - dayOfWeek) + resetDaysSorted[0];
}

int32_t InstanceCooltimeData::getDay(std::string_view day) {
	if (day == "Mon")
		return 1;
	else if (day == "Tue")
		return 2;
	else if (day == "Wed")
		return 3;
	else if (day == "Thu")
		return 4;
	else if (day == "Fri")
		return 5;
	else if (day == "Sat")
		return 6;
	else if (day == "Sun")
		return 7;
	throw runtime::IllegalArgumentException("Invalid Day: " + std::string(day));
}

int32_t InstanceCooltimeData::size() const {
	return static_cast<int32_t>(instanceCooltimes.size());
}

} // namespace aion::gameserver::dataholders
