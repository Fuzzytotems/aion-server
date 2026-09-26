#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/house/HouseDoorState.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/legion/LegionEmblemType.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/skillengine/model/TransformType.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::network::aion::serverpackets::detail {

/**
 * C++ only, private to the server packets (P4-17: L-Z and the Abstract* bases, which own this header, and P4-16: A-K): helpers the packet bodies
 * share.
 * <p>
 * The enum functions are the Java constructor data of enums that have no companion header yet (docs/design/static-data.md §2.5). They are
 * copied from the Java enums and stand in until the owning chunk writes the companion (tests compare them with the Java constants).
 */

/** Java: LegionEmblemType.getValue() - DEFAULT(0x00), CUSTOM(0x80) as a byte */
constexpr int8_t legionEmblemTypeValue(model::team::legion::LegionEmblemType type) noexcept {
	return type == model::team::legion::LegionEmblemType::CUSTOM ? static_cast<int8_t>(0x80) : int8_t{0};
}

/** Java: LegionRank.getRankId() - BRIGADE_GENERAL(0), DEPUTY(1), CENTURION(2), LEGIONARY(3), VOLUNTEER(4): equal to the ordinal */
constexpr int8_t legionRankId(model::team::legion::LegionRank rank) noexcept {
	return static_cast<int8_t>(rank);
}

/** Java: LegionHistoryAction.getId() (LegionHistoryAction.java constructor arguments in ordinal order) */
constexpr int8_t legionHistoryActionId(model::team::legion::LegionHistoryAction action) noexcept {
	static constexpr int8_t IDS[] = {0, 1, 2, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16, 17, 18};
	return IDS[static_cast<size_t>(action)];
}

/** Java: HouseDoorState.getId() - OPEN(1), CLOSED_EXCEPT_FRIENDS(2), CLOSED(3) */
constexpr int8_t houseDoorStateId(model::house::HouseDoorState state) noexcept {
	return static_cast<int8_t>(static_cast<int32_t>(state) + 1);
}

/** Java: SiegeRace.getRaceId() - ELYOS(Race.ELYOS: 0), ASMODIANS(Race.ASMODIANS: 1), BALAUR(2): equal to the ordinal */
constexpr int32_t siegeRaceId(model::siege::SiegeRace race) noexcept {
	return static_cast<int32_t>(race);
}

/** Java: QuestStatus.value() - START(3), REWARD(4), COMPLETE(5), LOCKED(6) */
constexpr int32_t questStatusValue(questEngine::model::QuestStatus status) noexcept {
	return static_cast<int32_t>(status) + 3;
}

/** Java: TransformType.getId() - NONE(0), PC(1), AVATAR(2), FORM1(3): equal to the ordinal */
constexpr int32_t transformTypeId(skillengine::model::TransformType type) noexcept {
	return static_cast<int32_t>(type);
}

/** Java: SummonMode.getId() - ATTACK(0), GUARD(1), REST(2), RELEASE(3), UNK(5) */
constexpr int32_t summonModeId(model::summons::SummonMode mode) noexcept {
	return mode == model::summons::SummonMode::UNK ? 5 : static_cast<int32_t>(mode);
}

/** Java: AbyssRankEnum.getId() - GRADE9_SOLDIER(1) ... SUPREME_COMMANDER(18): the ordinal + 1 */
constexpr int32_t abyssRankId(utils::stats::AbyssRankEnum rank) noexcept {
	return static_cast<int32_t>(rank) + 1;
}

/** Java: ItemPacketService.ItemAddType.getMask() (ItemPacketService.java constructor arguments in ordinal order) */
constexpr int32_t itemAddTypeMask(services::item::ItemPacketService_ItemAddType type) noexcept {
	static constexpr int32_t MASKS[] = {0x00, 0x07, 0x13, 0x19, 0x1C, 0x21, 0x23, 0x2B, 0x2D, 0x2E, 0x2F, 0x30, 0x35, 0x36, 0x36, 0x40, 0x50, 0x51};
	return MASKS[static_cast<size_t>(type)];
}

/** Java: ItemPacketService.ItemUpdateType constructor data (mask, sendable) in ordinal order */
struct ItemUpdateTypeData {
	int32_t mask;
	bool sendable;
};
constexpr ItemUpdateTypeData itemUpdateTypeData(services::item::ItemPacketService_ItemUpdateType type) noexcept {
	static constexpr ItemUpdateTypeData DATA[] = {{-1, false}, {-2, false}, {-3, false}, {0, true}, {0x01, true}, {0x05, true}, {0x06, true},
		{0x0A, true}, {0x13, true}, {0x16, true}, {0x17, true}, {0x19, true}, {0x1A, true}, {0x1C, true}, {0x1D, true}, {0x20, true}, {0x23, true},
		{0x25, true}, {0x32, true}, {0x49, true}, {0x4B, true}, {0x50, true}, {0x51, true}, {0x5A, true}, {0x5E, true}, {0x8A, true}};
	return DATA[static_cast<size_t>(type)];
}

/** Java: ItemPacketService.ItemDeleteType.getMask() (ItemPacketService.java constructor arguments in ordinal order) */
constexpr int32_t itemDeleteTypeMask(services::item::ItemPacketService_ItemDeleteType type) noexcept {
	// DEFAULT, SPLIT, MOVE, DISCARD, USE, SELL, QUEST_COMPLETE, QUEST_START, DECOMPOSE, REGISTER, PUT_TO_EXCHANGE
	static constexpr int32_t MASKS[] = {0x00, 0x04, 0x14, 0x15, 0x17, 0x1F, 0x31, 0x34, 0x66, 0x78, 0x26};
	return MASKS[static_cast<size_t>(type)];
}

/** Java: AttackStatus.getId() (AttackStatus.java constructor arguments in ordinal order) */
constexpr int32_t attackStatusId(controllers::attack::AttackStatus status) noexcept {
	static constexpr int32_t IDS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -64, -62, -60, -58, -54, -47, -45, -43, -41, -37};
	return IDS[static_cast<size_t>(status)];
}

/** Java: AttackStatus.isCounterSkill() - the dodge, parry, block and resist statuses of both hands, critical ones included */
constexpr bool attackStatusIsCounterSkill(controllers::attack::AttackStatus status) noexcept {
	using enum controllers::attack::AttackStatus;
	switch (status) {
		case BUF:
		case OFFHAND_BUF:
		case NORMALHIT:
		case OFFHAND_NORMALHIT:
		case CRITICAL:
		case OFFHAND_CRITICAL:
			return false;
		default:
			return true;
	}
}

/** Java: TeamType.getType() (TeamType.java constructor arguments in ordinal order) */
constexpr int32_t teamTypeType(model::team::TeamType type) noexcept {
	// GROUP, AUTO_GROUP, ALLIANCE, AUTO_ALLIANCE, ALLIANCE_DEFENCE, ALLIANCE_OFFENCE
	static constexpr int32_t TYPES[] = {0x3F, 0x02, 0x3F, 0x36, 0x3F, 0x02};
	return TYPES[static_cast<size_t>(type)];
}

/** Java: TeamType.getSubType() */
constexpr int32_t teamTypeSubType(model::team::TeamType type) noexcept {
	static constexpr int32_t SUB_TYPES[] = {0, 1, 0, 1, 4, 3};
	return SUB_TYPES[static_cast<size_t>(type)];
}

/** Java: LootRuleType.getId() - FREEFORALL(0), ROUNDROBIN(1), LEADER(2): equal to the ordinal */
constexpr int32_t lootRuleId(model::team::common::legacy::LootRuleType rule) noexcept {
	return static_cast<int32_t>(rule);
}

/** Java: SkillTargetSlot.FULLSLOTS (BUFF | DEBUFF | CHANT | SPEC | SPEC2 | BOOST | NOSHOW) */
inline constexpr int32_t SKILL_TARGET_SLOT_FULLSLOTS = 127;

/** Java: SkillTargetSlot.getId() - BUFF(1), DEBUFF(2), CHANT(4), SPEC(8), SPEC2(16), BOOST(32), NOSHOW(64), NONE(128): 1 << ordinal */
constexpr int32_t skillTargetSlotId(skillengine::model::SkillTargetSlot slot) noexcept {
	return 1 << static_cast<int32_t>(slot);
}

/** Java: effect.getTargetSlot(), dereferenced: NullPointerException for an effect without a target slot */
inline skillengine::model::SkillTargetSlot requireTargetSlot(std::optional<skillengine::model::SkillTargetSlot> slot) {
	if (!slot)
		throw runtime::NullPointerException("Effect.getTargetSlot() is null");
	return *slot;
}

/** Java auto-unboxing of a null Integer/Long/enum: NullPointerException */
template <class T>
T unbox(const std::optional<T>& value, std::string_view what) {
	if (!value)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/**
 * The entries of a Java `HashMap<Integer, V>` (or the elements of a `HashSet<Integer>`) in Java's iteration order, for packet members that hold
 * the map as an unordered container. The C++ container has lost the insertion order and the table history, so the keys are put in ascending
 * order: an approximation that equals Java's order only when Java's table has the size a new map of the same keys gets (no removals after it
 * grew) and keys sharing a bucket were inserted in ascending order (docs/deviations/P4-17.md).
 */
template <class V>
std::vector<std::pair<int32_t, V>> javaHashMapOrder(const std::unordered_map<int32_t, V>& map) {
	std::vector<std::pair<int32_t, V>> sorted(map.begin(), map.end());
	std::ranges::sort(sorted, {}, &std::pair<int32_t, V>::first);
	dataholders::detail::JavaHashMapOrder<int32_t, V> order;
	for (auto& [key, value] : sorted)
		order.put(key, value, dataholders::detail::javaHashCode(key));
	return order.entries();
}

inline std::vector<int32_t> javaHashSetOrder(const std::unordered_set<int32_t>& set) {
	std::unordered_map<int32_t, bool> map;
	for (int32_t element : set)
		map.emplace(element, true);
	std::vector<int32_t> keys;
	for (const auto& entry : javaHashMapOrder(map))
		keys.push_back(entry.first);
	return keys;
}

/**
 * The connection a PER_RECIPIENT packet is serialized for. Java dereferences `con` directly; a missing connection throws NullPointerException
 * naming the packet (runtime-architecture.md §8.7).
 */
inline AionConnection& requireConnection(AionConnection* con, std::string_view packetName) {
	if (con == nullptr)
		throw runtime::NullPointerException(std::string(packetName) + "::writeImpl without a connection");
	return *con;
}

} // namespace aion::gameserver::network::aion::serverpackets::detail
