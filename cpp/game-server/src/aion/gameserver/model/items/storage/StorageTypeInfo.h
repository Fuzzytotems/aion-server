#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aion/gameserver/model/items/storage/StorageType.h"

namespace aion::gameserver::model::items::storage {

/**
 * Companion of the generated enum StorageType (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions found by
 * ADL (`getLimit(storageType)` for Java `storageType.getLimit()`), the static constants as `inline constexpr`. Pure data, so it is ported: the
 * ItemStorage constructor (a layout constructor of every Storage) reads the limit.
 */

inline constexpr int32_t PET_BAG_MIN = 32;
inline constexpr int32_t PET_BAG_MAX = 43;
inline constexpr int32_t HOUSE_WH_MIN = 60;
inline constexpr int32_t HOUSE_WH_MAX = 79; // Custom cabinets ?? // since 3.0 to 4.0

namespace detail {
/** Java constructor arguments (id, limit, length, specialLimit) in ordinal order; omitted Java arguments are 0 */
struct StorageTypeData {
	int32_t id;
	int32_t limit;
	int32_t length;
	int32_t specialLimit;
};

inline constexpr std::array<StorageTypeData, 38> STORAGE_TYPE_DATA{{
	{0, 27, 9, 102}, // CUBE
	{1, 24, 8, 0},   // REGULAR_WAREHOUSE
	{2, 16, 8, 0},   // ACCOUNT_WAREHOUSE
	{3, 56, 8, 0},   // LEGION_WAREHOUSE
	{32, 6, 6, 0},   // PET_BAG_6
	{33, 12, 6, 0},  // PET_BAG_12
	{34, 18, 6, 0},  // PET_BAG_18
	{35, 24, 6, 0},  // PET_BAG_24
	{36, 12, 6, 0},  // CASH_PET_BAG_12
	{37, 18, 6, 0},  // CASH_PET_BAG_18
	{38, 30, 6, 0},  // CASH_PET_BAG_30
	{39, 24, 6, 0},  // CASH_PET_BAG_24
	{40, 30, 6, 0},  // PET_BAG_30
	{41, 26, 6, 0},  // CASH_PET_BAG_26
	{42, 32, 6, 0},  // CASH_PET_BAG_32
	{43, 34, 6, 0},  // CASH_PET_BAG_34
	{60, 9, 9, 0},   // HOUSE_STORAGE_01 Plain 1-Drawer Cabinet
	{61, 9, 9, 0},   // HOUSE_STORAGE_02 Simple 1-Drawer Cabinet
	{62, 9, 9, 0},   // HOUSE_STORAGE_03 Clean 1-Drawer Cabinet
	{63, 9, 9, 0},   // HOUSE_STORAGE_04 Convenient 1-Drawer Cabinet
	{64, 9, 9, 0},   // HOUSE_STORAGE_05 Strong 1-Drawer Cabinet
	{65, 9, 9, 0},   // HOUSE_STORAGE_06 Firm 1-Drawer Cabinet
	{66, 9, 9, 0},   // HOUSE_STORAGE_07 Fine 1-Drawer Cabinet
	{67, 9, 9, 0},   // HOUSE_STORAGE_08 Decorated 1-Drawer Cabinet
	{68, 18, 9, 0},  // HOUSE_STORAGE_09 Small 2-Drawer Cabinet
	{69, 18, 9, 0},  // HOUSE_STORAGE_10 Simple 2-Drawer Cabinet
	{70, 18, 9, 0},  // HOUSE_STORAGE_11 Clean 2-Drawer Cabinet
	{71, 18, 9, 0},  // HOUSE_STORAGE_12 Convenient 2-Drawer Cabinet
	{72, 18, 9, 0},  // HOUSE_STORAGE_13 Strong 2-Drawer Cabinet
	{73, 18, 9, 0},  // HOUSE_STORAGE_14 Firm 2-Drawer Cabinet
	{74, 27, 9, 0},  // HOUSE_STORAGE_15 Spacious 3-Drawer Cabinet
	{75, 27, 9, 0},  // HOUSE_STORAGE_16 Simple 3-Drawer Cabinet
	{76, 27, 9, 0},  // HOUSE_STORAGE_17 Clean 3-Drawer Cabinet
	{77, 27, 9, 0},  // HOUSE_STORAGE_18 Convenient 3-Drawer Cabinet
	{78, 27, 9, 0},  // HOUSE_STORAGE_19 Strong 3-Drawer Cabinet
	{79, 27, 9, 0},  // HOUSE_STORAGE_20 Firm 3-Drawer Cabinet
	{126, 0, 0, 0},  // BROKER
	{127, 0, 0, 0},  // MAILBOX
}};
static_assert(static_cast<size_t>(StorageType::MAILBOX) + 1 == STORAGE_TYPE_DATA.size(), "one entry per StorageType constant");

constexpr const StorageTypeData& storageTypeData(StorageType type) noexcept {
	return STORAGE_TYPE_DATA[static_cast<size_t>(type)];
}
} // namespace detail

constexpr int32_t getId(StorageType type) noexcept {
	return detail::storageTypeData(type).id;
}

constexpr int32_t getLimit(StorageType type) noexcept {
	return detail::storageTypeData(type).limit;
}

constexpr int32_t getLength(StorageType type) noexcept {
	return detail::storageTypeData(type).length;
}

constexpr int32_t getSpecialLimit(StorageType type) noexcept {
	return detail::storageTypeData(type).specialLimit;
}

/** @return the storage type with the id, null (std::nullopt) if there is none */
constexpr std::optional<StorageType> getStorageTypeById(int32_t id) noexcept {
	for (size_t i = 0; i < detail::STORAGE_TYPE_DATA.size(); ++i) {
		if (detail::STORAGE_TYPE_DATA[i].id == id)
			return static_cast<StorageType>(i);
	}
	return std::nullopt;
}

/** @return the id of the first storage type with the limit and length, -1 if there is none */
constexpr int32_t getStorageId(int32_t limit, int32_t length) noexcept {
	for (const detail::StorageTypeData& data : detail::STORAGE_TYPE_DATA) {
		if (data.limit == limit && data.length == length)
			return data.id;
	}
	return -1;
}

} // namespace aion::gameserver::model::items::storage
