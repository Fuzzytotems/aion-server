#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoHarmonyInstance.h"
#include "aion/gameserver/model/autogroup/AutoPvPFFAInstance.h"
#include "aion/gameserver/model/autogroup/AutoPvpInstance.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::autogroup {

namespace {

/** the class newAutoInstance creates (AutoGroupType.java:12-243) */
enum class InstanceKind : uint8_t { PVP, PVP_FFA, HARMONY };

struct ConstantData {
	int32_t instanceMaskId;
	int32_t maximumJoinTime;
	int8_t difficultId;
	InstanceKind kind;
};

/** the constructor arguments of the 33 constants in ordinal order (AutoGroupType.java:12-243) */
constexpr std::array<ConstantData, 33> CONSTANTS{{
	{1, 420000, 0, InstanceKind::PVP},           // BARANATH_DREDGION
	{2, 420000, 0, InstanceKind::PVP},           // CHANTRA_DREDGION
	{3, 420000, 0, InstanceKind::PVP},           // TERATH_DREDGION
	{21, 110000, 1, InstanceKind::PVP_FFA},      // ARENA_OF_CHAOS_1
	{22, 110000, 2, InstanceKind::PVP_FFA},      // ARENA_OF_CHAOS_2
	{23, 110000, 3, InstanceKind::PVP_FFA},      // ARENA_OF_CHAOS_3
	{24, 110000, 1, InstanceKind::PVP_FFA},      // ARENA_OF_DISCIPLINE_1
	{25, 110000, 2, InstanceKind::PVP_FFA},      // ARENA_OF_DISCIPLINE_2
	{26, 110000, 3, InstanceKind::PVP_FFA},      // ARENA_OF_DISCIPLINE_3
	{27, 110000, 1, InstanceKind::PVP_FFA},      // CHAOS_TRAINING_GROUNDS_1
	{28, 110000, 2, InstanceKind::PVP_FFA},      // CHAOS_TRAINING_GROUNDS_2
	{29, 110000, 3, InstanceKind::PVP_FFA},      // CHAOS_TRAINING_GROUNDS_3
	{30, 110000, 1, InstanceKind::PVP_FFA},      // DISCIPLINE_TRAINING_GROUNDS_1
	{31, 110000, 2, InstanceKind::PVP_FFA},      // DISCIPLINE_TRAINING_GROUNDS_2
	{32, 110000, 3, InstanceKind::PVP_FFA},      // DISCIPLINE_TRAINING_GROUNDS_3
	{33, 110000, 1, InstanceKind::HARMONY},      // ARENA_OF_HARMONY_1
	{34, 110000, 2, InstanceKind::HARMONY},      // ARENA_OF_HARMONY_2
	{35, 110000, 3, InstanceKind::HARMONY},      // ARENA_OF_HARMONY_3
	{38, 110000, 1, InstanceKind::PVP_FFA},      // ARENA_OF_GLORY_1
	{39, 110000, 4, InstanceKind::PVP_FFA},      // ARENA_OF_CHAOS_4
	{40, 110000, 4, InstanceKind::PVP_FFA},      // ARENA_OF_DISCIPLINE_4
	{41, 110000, 4, InstanceKind::HARMONY},      // ARENA_OF_HARMONY_4
	{42, 110000, 2, InstanceKind::PVP_FFA},      // ARENA_OF_GLORY_2
	{43, 110000, 4, InstanceKind::PVP_FFA},      // CHAOS_TRAINING_GROUNDS_4
	{44, 110000, 4, InstanceKind::PVP_FFA},      // DISCIPLINE_TRAINING_GROUNDS_4
	{45, 110000, 4, InstanceKind::HARMONY},      // HARAMONIOUS_TRAINING_CENTER_4
	{101, 110000, 1, InstanceKind::HARMONY},     // HARAMONIOUS_TRAINING_CENTER_1
	{102, 110000, 2, InstanceKind::HARMONY},     // HARAMONIOUS_TRAINING_CENTER_2
	{103, 110000, 3, InstanceKind::HARMONY},     // HARAMONIOUS_TRAINING_CENTER_3
	{107, 230000, 0, InstanceKind::PVP},         // KAMAR_BATTLEFIELD
	{108, 230000, 0, InstanceKind::PVP},         // ENGULFED_OPHIDIAN_BRIDGE
	{109, 230000, 0, InstanceKind::PVP},         // IRON_WALL_WARFRONT
	{111, 170000, 0, InstanceKind::PVP},         // IDGEL_DOME
}};

static_assert(CONSTANTS.size() == xml::EnumTraits<AutoGroupType>::names.size(), "one row per AutoGroupType constant");

const ConstantData& dataOf(AutoGroupType agt) {
	return CONSTANTS.at(static_cast<size_t>(agt));
}

/** Java: the template field, dereferenced (null for a mask id without template: Java's NullPointerException) */
const AutoGroup& templateOf(AutoGroupType agt) {
	const AutoGroup* found = getTemplate(agt);
	if (found == nullptr)
		throw runtime::NullPointerException("AutoGroupType " + std::string(xml::EnumTraits<AutoGroupType>::names.at(static_cast<size_t>(agt))) +
			" has no auto group template (instanceMaskId=" + std::to_string(dataOf(agt).instanceMaskId) + ")");
	return *found;
}

/** Java: values() in ordinal order */
template <class Predicate>
std::optional<AutoGroupType> firstOf(Predicate predicate) {
	for (size_t ordinal = 0; ordinal < CONSTANTS.size(); ordinal++) {
		const AutoGroupType agt = static_cast<AutoGroupType>(ordinal);
		if (predicate(agt))
			return agt;
	}
	return std::nullopt;
}

} // namespace

int32_t getInstanceMaskId(AutoGroupType agt) {
	return dataOf(agt).instanceMaskId;
}

// Java AutoGroupType.java:254-263 (the constructor's lookup)
const AutoGroup* getTemplate(AutoGroupType agt) {
	return dataholders::DataManager::AUTO_GROUP->getTemplateByInstanceMaskId(dataOf(agt).instanceMaskId);
}

// Java AutoGroupType.java:265-268
int32_t getL10nId(AutoGroupType agt) {
	return templateOf(agt).getL10nId();
}

// Java AutoGroupType.java:270-272
int32_t getMaximumJoinTime(AutoGroupType agt) {
	return dataOf(agt).maximumJoinTime;
}

// Java AutoGroupType.java:274-276
bool containNpcId(AutoGroupType agt, int32_t npcId) {
	const std::vector<int32_t>& npcIds = templateOf(agt).getNpcIds();
	return std::find(npcIds.begin(), npcIds.end(), npcId) != npcIds.end();
}

// Java AutoGroupType.java:278-281
bool isPeriodicInstance(AutoGroupType agt) {
	return agt == AutoGroupType::KAMAR_BATTLEFIELD || agt == AutoGroupType::ENGULFED_OPHIDIAN_BRIDGE || agt == AutoGroupType::IDGEL_DOME ||
		agt == AutoGroupType::IRON_WALL_WARFRONT || agt == AutoGroupType::CHANTRA_DREDGION || agt == AutoGroupType::BARANATH_DREDGION ||
		agt == AutoGroupType::TERATH_DREDGION;
}

// Java AutoGroupType.java:283-290
std::optional<AutoGroupType> getAGTByMaskId(int32_t instanceMaskId) {
	return firstOf([instanceMaskId](AutoGroupType agt) { return templateOf(agt).getMaskId() == instanceMaskId; });
}

// Java AutoGroupType.java:292-299
std::optional<AutoGroupType> getAutoGroup(int32_t level, int32_t npcId) {
	return firstOf([level, npcId](AutoGroupType agt) { return isInLvlRange(agt, level) && containNpcId(agt, npcId); });
}

// Java AutoGroupType.java:301-308
std::optional<AutoGroupType> getAutoGroupByWorld(int32_t level, int32_t worldId) {
	return firstOf([level, worldId](AutoGroupType agt) { return templateOf(agt).getInstanceMapId() == worldId && isInLvlRange(agt, level); });
}

// Java AutoGroupType.java:310-317
std::optional<AutoGroupType> getAutoGroup(int32_t npcId) {
	return firstOf([npcId](AutoGroupType agt) { return containNpcId(agt, npcId); });
}

// Java AutoGroupType.java:319-324
bool isPvPSoloArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::ARENA_OF_DISCIPLINE_1:
		case AutoGroupType::ARENA_OF_DISCIPLINE_2:
		case AutoGroupType::ARENA_OF_DISCIPLINE_3:
		case AutoGroupType::ARENA_OF_DISCIPLINE_4:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:326-331
bool isTrainingPvPSoloArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::DISCIPLINE_TRAINING_GROUNDS_1:
		case AutoGroupType::DISCIPLINE_TRAINING_GROUNDS_2:
		case AutoGroupType::DISCIPLINE_TRAINING_GROUNDS_3:
		case AutoGroupType::DISCIPLINE_TRAINING_GROUNDS_4:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:333-338
bool isPvPFFAArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::ARENA_OF_CHAOS_1:
		case AutoGroupType::ARENA_OF_CHAOS_2:
		case AutoGroupType::ARENA_OF_CHAOS_3:
		case AutoGroupType::ARENA_OF_CHAOS_4:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:340-345
bool isTrainingPvPFFAArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::CHAOS_TRAINING_GROUNDS_1:
		case AutoGroupType::CHAOS_TRAINING_GROUNDS_2:
		case AutoGroupType::CHAOS_TRAINING_GROUNDS_3:
		case AutoGroupType::CHAOS_TRAINING_GROUNDS_4:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:347-352
bool isTrainingHarmonyArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::HARAMONIOUS_TRAINING_CENTER_1:
		case AutoGroupType::HARAMONIOUS_TRAINING_CENTER_2:
		case AutoGroupType::HARAMONIOUS_TRAINING_CENTER_3:
		case AutoGroupType::HARAMONIOUS_TRAINING_CENTER_4:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:354-359
bool isHarmonyArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::ARENA_OF_HARMONY_1:
		case AutoGroupType::ARENA_OF_HARMONY_2:
		case AutoGroupType::ARENA_OF_HARMONY_3:
		case AutoGroupType::ARENA_OF_HARMONY_4:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:361-366
bool isGloryArena(AutoGroupType agt) {
	switch (agt) {
		case AutoGroupType::ARENA_OF_GLORY_1:
		case AutoGroupType::ARENA_OF_GLORY_2:
			return true;
		default:
			return false;
	}
}

// Java AutoGroupType.java:368-370
bool isPvpArena(AutoGroupType agt) {
	return isTrainingPvPFFAArena(agt) || isPvPFFAArena(agt) || isTrainingPvPSoloArena(agt) || isPvPSoloArena(agt);
}

// Java AutoGroupType.java:372-374
bool isInLvlRange(AutoGroupType agt, int32_t level) {
	const AutoGroup& found = templateOf(agt);
	return level >= found.getMinLvl() && level <= found.getMaxLvl();
}

// Java AutoGroupType.java:376-378
int8_t getDifficultId(AutoGroupType agt) {
	return dataOf(agt).difficultId;
}

// Java AutoGroupType.java:380-384 and the constant-specific newAutoInstance bodies (:12-243)
runtime::Ref<AutoInstance> createAutoInstance(AutoGroupType agt) {
	// Java: agt.newAutoInstance() - the 33 constant bodies, each of one of the three kinds
	switch (dataOf(agt).kind) {
		case InstanceKind::PVP:
			return AutoPvpInstance::create(agt);
		case InstanceKind::PVP_FFA:
			return AutoPvPFFAInstance::create(agt);
		case InstanceKind::HARMONY:
			return AutoHarmonyInstance::create(agt);
	}
	throw runtime::NullPointerException("AutoGroupType without newAutoInstance");
}

} // namespace aion::gameserver::model::autogroup
