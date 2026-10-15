#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Neon
 */
class EventDAO {
public:
	/**
	 * C++ note: the Java sets are null when the database column is NULL (parseInts), so they are optional here.
	 */
	class StoredBuffData {
	private:
		int32_t buffIndex{};
		std::optional<std::unordered_set<int32_t>> activePoolSkillIds{};
		std::optional<std::unordered_set<int32_t>> allowedBuffDays{};
	public:
		StoredBuffData(int32_t buffIndex, std::optional<std::unordered_set<int32_t>> activePoolSkillIds,
			std::optional<std::unordered_set<int32_t>> allowedBuffDays);
		int32_t getBuffIndex() const { return this->buffIndex; }
		const std::optional<std::unordered_set<int32_t>>& getActivePoolSkillIds() const { return this->activePoolSkillIds; }
		const std::optional<std::unordered_set<int32_t>>& getAllowedBuffDays() const { return this->allowedBuffDays; }
	};

	static void deleteOldBuffData();
	/** @return absent (Java null) if the data could not be loaded */
	static std::optional<std::vector<EventDAO::StoredBuffData>> loadStoredBuffData(std::string_view eventName);
	static bool storeBuffData(std::string_view eventName, const std::vector<EventDAO::StoredBuffData>& storedBuffData);
private:
	static std::optional<std::string> serialize(const std::optional<std::unordered_set<int32_t>>& ints);
	static std::optional<std::unordered_set<int32_t>> parseInts(std::optional<std::string_view> string);
};

} // namespace aion::gameserver::dao
