#include "aion/gameserver/skillengine/condition/WeaponCondition.h"

#include <algorithm>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::item::enums::ItemGroup;
using runtime::Ptr;

bool WeaponCondition::validate(model::Skill& env) const {
	if (env.getSkillMethod() != model::Skill::SkillMethod::CAST)
		return true;
	return isValidWeapon(env.getEffector());
}

bool WeaponCondition::validate(gameserver::model::stats::calc::Stat2& stat,
	gameserver::model::stats::calc::functions::IStatFunction& /*statFunction*/) const {
	return isValidWeapon(stat.getOwner());
}

bool WeaponCondition::isValidWeapon(Ptr<Creature> creature) const {
	if (Ptr<Player> player = runtime::as<Player>(creature)) {
		if (!itemGroups.has_value()) // Java: itemGroups.contains(...) on the null `weapon` attribute throws
			throw runtime::NullPointerException("Cannot invoke \"java.util.List.contains(Object)\" because \"this.itemGroups\" is null");
		// Java List.contains(null) is false for a list of enum constants, so an empty main hand never matches
		std::optional<ItemGroup> mainHandWeaponType = player->getEquipment().getMainHandWeaponType();
		return mainHandWeaponType.has_value() && std::ranges::find(*itemGroups, *mainHandWeaponType) != itemGroups->end();
	}
	// for npcs we don't validate weapon, though in templates they are present
	return true;
}

} // namespace aion::gameserver::skillengine::condition
