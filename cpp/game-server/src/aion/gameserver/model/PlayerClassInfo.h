#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/templates/stats/fwd.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum PlayerClass (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions found by
 * ADL (`getClassId(playerClass)` for Java `playerClass.getClassId()`). Java's PlayerClass implements L10n (`getL10nId(playerClass)`).
 * <p>
 * createStatsTemplate (with the nested Java class PlayerStatsTemplate, defined in the .cpp) interns one immutable template per class and level
 * (see its comment), so the `const StatsTemplate*` PlayerGameStats keeps is immortal like every template.
 */

namespace detail {
/** Java constructor arguments in ordinal order; startingClass is the ordinal of the starting class (a starting class names itself) */
struct PlayerClassData {
	int8_t classId;
	int32_t nameId;
	uint8_t startingClass;
	int32_t power, health, agility, accuracy, knowledge, will, healthMultiplier, willMultiplier, magicalCriticalResist;
};

inline constexpr std::array<PlayerClassData, 17> PLAYER_CLASS_DATA{{
	{0, 240000, 0, 110, 110, 100, 100, 90, 90, 400, 400, 0},     // WARRIOR (starting class)
	{1, 240001, 0, 115, 115, 100, 100, 90, 90, 440, 400, 0},     // GLADIATOR (fighter)
	{2, 240002, 0, 115, 100, 100, 100, 90, 105, 460, 400, 0},    // TEMPLAR (knight)
	{3, 240003, 3, 100, 100, 110, 110, 90, 90, 360, 400, 0},     // SCOUT (starting class)
	{4, 240004, 3, 110, 100, 110, 110, 90, 90, 360, 400, 0},     // ASSASSIN
	{5, 240005, 3, 100, 100, 115, 115, 90, 90, 280, 400, 0},     // RANGER
	{6, 240006, 6, 90, 90, 95, 95, 115, 115, 260, 600, 0},       // MAGE (starting class)
	{7, 240007, 6, 90, 90, 100, 100, 120, 110, 260, 600, 50},    // SORCERER (wizard)
	{8, 240008, 6, 90, 90, 100, 100, 115, 115, 280, 600, 50},    // SPIRIT_MASTER (elementalist)
	{9, 240009, 9, 95, 95, 100, 100, 100, 100, 360, 600, 0},     // PRIEST (starting class)
	{10, 240010, 9, 105, 110, 90, 90, 105, 110, 320, 600, 50},   // CLERIC
	{11, 240011, 9, 110, 105, 90, 90, 105, 110, 360, 600, 0},    // CHANTER
	{12, 904314, 12, 100, 100, 110, 110, 90, 90, 360, 400, 0},   // ENGINEER (starting class)
	{13, 904315, 12, 100, 100, 100, 100, 105, 105, 420, 480, 0}, // RIDER
	{14, 904316, 12, 100, 105, 105, 100, 100, 100, 360, 400, 0}, // GUNNER
	{15, 904317, 15, 95, 95, 100, 100, 100, 105, 320, 600, 0},   // ARTIST (starting class)
	{16, 904318, 15, 90, 100, 100, 100, 110, 110, 320, 520, 50}, // BARD
}};
static_assert(static_cast<size_t>(PlayerClass::BARD) + 1 == PLAYER_CLASS_DATA.size(), "one entry per PlayerClass constant");

constexpr const PlayerClassData& playerClassData(PlayerClass playerClass) noexcept {
	return PLAYER_CLASS_DATA[static_cast<size_t>(playerClass)];
}
} // namespace detail

/** Java: PlayerClass.createStatsTemplate(int level). C++: the template is interned per class and level (never destroyed). */
const templates::stats::StatsTemplate* createStatsTemplate(PlayerClass playerClass, int32_t level);

/** Java: PlayerClass.getClassId() - the id used on client side */
constexpr int8_t getClassId(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).classId;
}

/**
 * Java: PlayerClass.getPlayerClassById(byte, boolean ignoreInvalidClassId) with ignoreInvalidClassId
 *
 * @return the player class with the id, null (std::nullopt) if there is none and ignoreInvalidClassId is true
 * @throws IllegalArgumentException
 *           if there is none and ignoreInvalidClassId is false
 */
std::optional<PlayerClass> getPlayerClassById(int8_t classId, bool ignoreInvalidClassId);

/**
 * Java: PlayerClass.getPlayerClassById(byte)
 *
 * @throws IllegalArgumentException
 *           if there is no player class with the id
 */
PlayerClass getPlayerClassById(int8_t classId);

/** Java: PlayerClass.getL10nId() (L10n) */
constexpr int32_t getL10nId(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).nameId;
}

/** Java: PlayerClass.isStartingClass() - true if a player can create a character with this class */
constexpr bool isStartingClass(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).startingClass == static_cast<uint8_t>(playerClass);
}

/** Java: PlayerClass.getStartingClass() */
constexpr PlayerClass getStartingClass(PlayerClass playerClass) noexcept {
	return static_cast<PlayerClass>(detail::playerClassData(playerClass).startingClass);
}

/** Java: PlayerClass.isPhysicalClass() */
constexpr bool isPhysicalClass(PlayerClass playerClass) noexcept {
	switch (playerClass) {
		case PlayerClass::WARRIOR:
		case PlayerClass::GLADIATOR:
		case PlayerClass::TEMPLAR:
		case PlayerClass::SCOUT:
		case PlayerClass::ASSASSIN:
		case PlayerClass::RANGER:
		case PlayerClass::CHANTER:
			return true;
		default:
			return false;
	}
}

/** Java: PlayerClass.getIconImage() */
constexpr std::string_view getIconImage(PlayerClass playerClass) noexcept {
	switch (playerClass) {
		case PlayerClass::WARRIOR:
			return "textures/ui/EMBLEM/icon_emblem_warrior.dds";
		case PlayerClass::GLADIATOR:
			return "textures/ui/EMBLEM/icon_emblem_fighter.dds";
		case PlayerClass::TEMPLAR:
			return "textures/ui/EMBLEM/icon_emblem_knight.dds";
		case PlayerClass::SCOUT:
			return "textures/ui/EMBLEM/icon_emblem_scout.dds";
		case PlayerClass::ASSASSIN:
			return "textures/ui/EMBLEM/icon_emblem_assassin.dds";
		case PlayerClass::RANGER:
			return "textures/ui/EMBLEM/icon_emblem_ranger.dds";
		case PlayerClass::MAGE:
			return "textures/ui/EMBLEM/icon_emblem_mage.dds";
		case PlayerClass::SORCERER:
			return "textures/ui/EMBLEM/icon_emblem_wizard.dds";
		case PlayerClass::SPIRIT_MASTER:
			return "textures/ui/EMBLEM/icon_emblem_elementalist.dds";
		case PlayerClass::PRIEST:
			return "textures/ui/EMBLEM/icon_emblem_cleric.dds"; // cleric and priest images are switched in client
		case PlayerClass::CLERIC:
			return "textures/ui/EMBLEM/icon_emblem_priest.dds"; // cleric and priest images are switched in client
		case PlayerClass::CHANTER:
			return "textures/ui/EMBLEM/icon_emblem_chanter.dds";
		case PlayerClass::ENGINEER:
			return "textures/ui/EMBLEM/Icon_emblem_Engineer.dds";
		case PlayerClass::RIDER:
			return "textures/ui/EMBLEM/Icon_emblem_Rider.dds";
		case PlayerClass::GUNNER:
			return "textures/ui/EMBLEM/Icon_emblem_Gunner.dds";
		case PlayerClass::ARTIST:
			return "textures/ui/EMBLEM/Icon_emblem_Artist.dds";
		case PlayerClass::BARD:
			return "textures/ui/EMBLEM/Icon_emblem_Bard.dds";
	}
	return {}; // unreachable: Java's switch expression is exhaustive
}

constexpr int32_t getPower(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).power;
}

constexpr int32_t getHealth(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).health;
}

constexpr int32_t getAgility(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).agility;
}

constexpr int32_t getAccuracy(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).accuracy;
}

constexpr int32_t getKnowledge(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).knowledge;
}

constexpr int32_t getWill(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).will;
}

constexpr int32_t getWillMultiplier(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).willMultiplier;
}

constexpr int32_t getHealthMultiplier(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).healthMultiplier;
}

constexpr int32_t getAgilityMultiplier(PlayerClass) noexcept {
	return 310;
}

constexpr int32_t getAccuracyMultiplier(PlayerClass) noexcept {
	return 200;
}

constexpr int32_t getNoWeaponPowerMultiplier(PlayerClass) noexcept {
	return 70;
}

/** C++ only: Java's private field magicalCriticalResist (read by createStatsTemplate as the spell resist) */
constexpr int32_t getMagicalCriticalResist(PlayerClass playerClass) noexcept {
	return detail::playerClassData(playerClass).magicalCriticalResist;
}

} // namespace aion::gameserver::model
