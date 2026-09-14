#include "aion/gameserver/dao/EventDAO.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT `buff_index`, `buff_active_pool_ids`, `buff_allowed_days` FROM `event` WHERE `event_name`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `event` (`event_name`, `buff_index`, `buff_active_pool_ids`, `buff_allowed_days`) VALUES (?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `event` WHERE `event_name`=?";
constexpr std::string_view DELETE_OLD_QUERY = "DELETE FROM `event` WHERE last_change < ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.EventDAO");

EventDAO::StoredBuffData::StoredBuffData(int32_t buffIndexValue, std::optional<std::unordered_set<int32_t>> activePoolSkillIdsValue,
	std::optional<std::unordered_set<int32_t>> allowedBuffDaysValue)
	: buffIndex(buffIndexValue), activePoolSkillIds(std::move(activePoolSkillIdsValue)), allowedBuffDays(std::move(allowedBuffDaysValue)) {
}

void EventDAO::deleteOldBuffData() {
	AION_UNPORTED();
}

std::optional<std::vector<EventDAO::StoredBuffData>> EventDAO::loadStoredBuffData(std::string_view eventName) {
	AION_UNPORTED();
}

bool EventDAO::storeBuffData(std::string_view eventName, const std::vector<EventDAO::StoredBuffData>& storedBuffData) {
	AION_UNPORTED();
}

std::optional<std::string> EventDAO::serialize(const std::optional<std::unordered_set<int32_t>>& ints) {
	AION_UNPORTED();
}

std::optional<std::unordered_set<int32_t>> EventDAO::parseInts(std::optional<std::string_view> string) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
