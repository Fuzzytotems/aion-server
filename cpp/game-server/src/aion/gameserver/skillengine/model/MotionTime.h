#pragma once

#include "aion/gameserver/skillengine/model/MotionTime.xml.h"

#include <cstdint>
#include <map>
#include <optional>
#include <utility>

#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.MotionTime: the animation times of one motion per race, gender and weapon.
 * <p>
 * C++ notes (P4-08): the @XmlTransient maps are private members filled by afterUnmarshal; their values point into the bound `Times` lists,
 * which stay (static-data.md §2.6, Java sets them to null). Java's `WeaponTypeWrapper` key (a P5-02 class without a C++ header yet) is the
 * normalized (main hand, off hand) pair `WeaponKey`, built by `weaponTypeWrapper` exactly like its constructor; the maps are only looked up,
 * never iterated, so the ordered map replaces the HashMap without an observable difference.
 */
class MotionTime : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/MotionTime.xml.inc"
private:
	/** Java WeaponTypeWrapper(mainHand, offHand): nullopt for Java null */
	using WeaponKey = std::pair<std::optional<gameserver::model::templates::item::enums::ItemGroup>,
		std::optional<gameserver::model::templates::item::enums::ItemGroup>>;
	using TimesById = std::map<int32_t, const Times*>;

	std::map<WeaponKey, TimesById> asmodianFemaleTimeForWeaponType;
	std::map<WeaponKey, TimesById> asmodianMaleTimeForWeaponType;
	std::map<WeaponKey, TimesById> elyosFemaleTimeForWeaponType;
	std::map<WeaponKey, TimesById> elyosMaleTimeForWeaponType;

public:
	/** Java package-private field robotTimes */
	TimesById robotTimes;

	/**
	 * @return in robot mode the robot times of the highest id <= `id`; otherwise the times of exactly `id` in the map of the player's race,
	 * gender and (main hand, off hand) weapon types, without falling back to lower ids (Java returns times.get(id) of the first weapon map it
	 * finds). nullptr (Java null) if the map or the id is missing
	 */
	const Times* getTimesFor(gameserver::model::gameobjects::player::Player& player, int32_t id) const;

	/**
	 * C++ only: the body of getTimesFor(Player, id) over the player state it reads (robot mode, race, gender, main and off hand weapon type), so
	 * the lookup can be tested without a Player. Java reads isInRobotMode() again in every loop iteration; the player's state is read once here.
	 */
	const Times* getTimesFor(bool inRobotMode, gameserver::model::Race race, gameserver::model::Gender gender,
		std::optional<gameserver::model::templates::item::enums::ItemGroup> mainHand,
		std::optional<gameserver::model::templates::item::enums::ItemGroup> offHand, int32_t id) const;

	/** C++ only: the normalization of the Java WeaponTypeWrapper constructor */
	static WeaponKey weaponTypeWrapper(std::optional<gameserver::model::templates::item::enums::ItemGroup> mainHand,
		std::optional<gameserver::model::templates::item::enums::ItemGroup> offHand);

private:
	void parseTimesFrom(const std::vector<Times>& times, std::map<WeaponKey, TimesById>& map);
};

} // namespace aion::gameserver::skillengine::model
