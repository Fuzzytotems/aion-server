#include "aion/gameserver/skillengine/model/MotionTime.h"

#include <string>
#include <string_view>

#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::skillengine::model {

using gameserver::model::templates::item::enums::ItemGroup;

const Times* MotionTime::getTimesFor(gameserver::model::gameobjects::player::Player& player, int32_t id) const {
	if (player.isInRobotMode())
		return getTimesFor(true, player.getRace(), player.getGender(), std::nullopt, std::nullopt, id);
	return getTimesFor(false, player.getRace(), player.getGender(), player.getEquipment().getMainHandWeaponType(),
		player.getEquipment().getOffHandWeaponType(), id);
}

const Times* MotionTime::getTimesFor(bool inRobotMode, gameserver::model::Race race, gameserver::model::Gender gender,
	std::optional<ItemGroup> mainHand, std::optional<ItemGroup> offHand, int32_t id) const {
	using gameserver::model::Gender;
	using gameserver::model::Race;
	const WeaponKey weapons = weaponTypeWrapper(mainHand, offHand); // Java: null in robot mode, where it is not used
	for (int32_t i = id; i > 0; i--) {
		if (inRobotMode) {
			auto it = robotTimes.find(i);
			if (it != robotTimes.end())
				return it->second;
		} else {
			const std::map<WeaponKey, TimesById>* byWeapon = nullptr;
			switch (race) {
				case Race::ASMODIANS:
					byWeapon = gender == Gender::FEMALE ? &asmodianFemaleTimeForWeaponType : &asmodianMaleTimeForWeaponType;
					break;
				case Race::ELYOS:
					byWeapon = gender == Gender::FEMALE ? &elyosFemaleTimeForWeaponType : &elyosMaleTimeForWeaponType;
					break;
				default:
					break;
			}
			if (byWeapon == nullptr) // Java: times stays null for other races
				continue;
			auto times = byWeapon->find(weapons);
			if (times != byWeapon->end()) {
				// Java returns times.get(i) even when it is null, without trying lower ids
				auto it = times->second.find(i);
				return it != times->second.end() ? it->second : nullptr;
			}
		}
	}
	return nullptr;
}

void MotionTime::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	parseTimesFrom(asmodianFemale, asmodianFemaleTimeForWeaponType);
	parseTimesFrom(asmodianMale, asmodianMaleTimeForWeaponType);
	parseTimesFrom(elyosFemale, elyosFemaleTimeForWeaponType);
	parseTimesFrom(elyosMale, elyosMaleTimeForWeaponType);
	for (const Times& time : robot)
		robotTimes.insert_or_assign(time.getId(), &time);
	// Java: the five lists = null (the C++ maps point into the lists, which stay)
}

void MotionTime::parseTimesFrom(const std::vector<Times>& times, std::map<WeaponKey, TimesById>& map) {
	for (const Times& t : times) {
		WeaponKey wrapper;
		std::string_view weapon = t.getWeapon();
		if (weapon == "1hand") {
			wrapper = weaponTypeWrapper(ItemGroup::SWORD, std::nullopt);
		} else if (weapon == "2hand") {
			wrapper = weaponTypeWrapper(ItemGroup::GREATSWORD, std::nullopt);
		} else if (weapon == "keyblade") {
			wrapper = weaponTypeWrapper(ItemGroup::KEYBLADE, std::nullopt);
		} else if (weapon == "polearm") {
			wrapper = weaponTypeWrapper(ItemGroup::POLEARM, std::nullopt);
		} else if (weapon == "dagger") {
			wrapper = weaponTypeWrapper(ItemGroup::DAGGER, std::nullopt);
		} else if (weapon == "mace") {
			wrapper = weaponTypeWrapper(ItemGroup::MACE, std::nullopt);
		} else if (weapon == "staff") {
			wrapper = weaponTypeWrapper(ItemGroup::STAFF, std::nullopt);
		} else if (weapon == "2weapon") {
			wrapper = weaponTypeWrapper(ItemGroup::DAGGER, ItemGroup::DAGGER);
			map[weaponTypeWrapper(ItemGroup::SWORD, ItemGroup::SWORD)].insert_or_assign(t.getId(), &t);
			map[weaponTypeWrapper(ItemGroup::MACE, ItemGroup::MACE)].insert_or_assign(t.getId(), &t);
			// other combinations don't need to be added, as the WeaponTypeWrapper constructor already limits them
		} else if (weapon == "noweapon") {
			wrapper = weaponTypeWrapper(std::nullopt, std::nullopt);
		} else if (weapon == "book") {
			wrapper = weaponTypeWrapper(ItemGroup::SPELLBOOK, std::nullopt);
		} else if (weapon == "orb") {
			wrapper = weaponTypeWrapper(ItemGroup::ORB, std::nullopt);
		} else if (weapon == "1gun") {
			wrapper = weaponTypeWrapper(ItemGroup::GUN, std::nullopt);
		} else if (weapon == "2gun") {
			wrapper = weaponTypeWrapper(ItemGroup::GUN, ItemGroup::GUN);
		} else if (weapon == "cannon") {
			wrapper = weaponTypeWrapper(ItemGroup::CANNON, std::nullopt);
		} else if (weapon == "bow") {
			wrapper = weaponTypeWrapper(ItemGroup::BOW, std::nullopt);
		} else if (weapon == "harp") {
			wrapper = weaponTypeWrapper(ItemGroup::HARP, std::nullopt);
		} else {
			throw runtime::IllegalArgumentException(std::string(weapon) + " is not implemented");
		}
		map[wrapper].insert_or_assign(t.getId(), &t);
	}
}

MotionTime::WeaponKey MotionTime::weaponTypeWrapper(std::optional<ItemGroup> mainHand, std::optional<ItemGroup> offHand) {
	if (mainHand && offHand) {
		switch (*mainHand) {
			case ItemGroup::DAGGER:
			case ItemGroup::SWORD:
			case ItemGroup::MACE:
			case ItemGroup::TOOLHOES:
			case ItemGroup::GUN:
				return {mainHand, mainHand};
			default:
				return {mainHand, std::nullopt};
		}
	}
	return {mainHand, offHand};
}

} // namespace aion::gameserver::skillengine::model
