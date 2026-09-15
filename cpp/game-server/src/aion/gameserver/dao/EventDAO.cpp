#include "aion/gameserver/dao/EventDAO.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/templates/detail/JavaHashMap.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

namespace {

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
	try {
		auto con = DatabaseFactory::getConnection();
		auto deleteStatement = con->prepareStatement(DELETE_OLD_QUERY);
		// Java: ServerTime.now().with(LocalTime.MIDNIGHT).withDayOfMonth(1)
		const std::chrono::local_days today = std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time());
		const std::chrono::year_month_day date{today};
		const std::chrono::local_days firstOfMonth{date.year() / date.month() / std::chrono::day{1}};
		const utils::time::ServerTime::ZonedDateTime startOfMonth =
			utils::time::ServerTime::of(std::chrono::time_point_cast<std::chrono::milliseconds>(firstOfMonth));
		deleteStatement->setTimestamp(1, startOfMonth.get_sys_time());
		deleteStatement->execute();
	} catch (const std::exception& e) {
		log.error("Couldn't clean up old event buff data", e);
	}
}

std::optional<std::vector<EventDAO::StoredBuffData>> EventDAO::loadStoredBuffData(std::string_view eventName) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setString(1, eventName);
		auto rset = stmt->executeQuery();
		if (!rset->isAfterLast()) {
			std::vector<StoredBuffData> storedBuffData;
			while (rset->next()) {
				int32_t buffIndex = rset->getInt("buff_index");
				std::optional<std::string> activePoolSkillIdsText = rset->getObject<std::string>("buff_active_pool_ids");
				std::optional<std::unordered_set<int32_t>> activePoolSkillIds =
					parseInts(activePoolSkillIdsText ? std::optional<std::string_view>(*activePoolSkillIdsText) : std::nullopt);
				std::optional<std::string> activeRandomDaysText = rset->getObject<std::string>("buff_allowed_days");
				std::optional<std::unordered_set<int32_t>> activeRandomDays =
					parseInts(activeRandomDaysText ? std::optional<std::string_view>(*activeRandomDaysText) : std::nullopt);
				storedBuffData.emplace_back(buffIndex, std::move(activePoolSkillIds), std::move(activeRandomDays));
			}
			return storedBuffData;
		}
	} catch (const SQLException& e) {
		log.error("Couldn't load stored event buff info for event: " + std::string(eventName), e);
	}
	return std::nullopt;
}

bool EventDAO::storeBuffData(std::string_view eventName, const std::vector<EventDAO::StoredBuffData>& storedBuffData) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto delStmt = con->prepareStatement(DELETE_QUERY);
		auto stmt = con->prepareStatement(INSERT_QUERY);
		delStmt->setString(1, eventName);
		delStmt->execute(); // delete all old entries first
		for (const StoredBuffData& data : storedBuffData) { // store new entries
			stmt->setString(1, eventName);
			stmt->setInt(2, data.getBuffIndex());
			stmt->setString(3, serialize(data.getActivePoolSkillIds()));
			stmt->setString(4, serialize(data.getAllowedBuffDays()));
			stmt->addBatch();
		}
		stmt->executeBatch();
		return true;
	} catch (const std::exception& e) {
		log.error("Couldn't store event buff info for event: " + std::string(eventName), e);
		return false;
	}
}

std::optional<std::string> EventDAO::serialize(const std::optional<std::unordered_set<int32_t>>& ints) {
	if (!ints)
		return std::nullopt;
	// Java: ints.stream().map(String::valueOf).collect(Collectors.joining(",")) iterates the HashSet in bucket order; the keys are put in ascending
	// order into the HashSet model (an unordered_set keeps no insertion order), which gives Java's order for every set that has no two keys in one
	// bucket with a different insertion order
	std::vector<int32_t> sorted(ints->begin(), ints->end());
	std::ranges::sort(sorted);
	std::vector<std::pair<int32_t, int32_t>> puts;
	puts.reserve(sorted.size());
	for (int32_t value : sorted)
		puts.emplace_back(value, value);
	std::string joined;
	for (int32_t value : model::templates::detail::javaIntegerHashMapValues(puts)) {
		if (!joined.empty())
			joined += ',';
		joined += std::to_string(value);
	}
	return joined;
}

std::optional<std::unordered_set<int32_t>> EventDAO::parseInts(std::optional<std::string_view> string) {
	if (!string)
		return std::nullopt;
	// Java: Stream.of(string.split(",")).map(Integer::parseInt) (so "" gives one empty token, which Integer.parseInt rejects)
	std::vector<std::string_view> tokens = detail::javaSplit(*string, ',');
	std::unordered_set<int32_t> ints;
	for (std::string_view token : tokens)
		ints.insert(commons::utils::parseInt(token));
	return ints;
}

} // namespace aion::gameserver::dao
