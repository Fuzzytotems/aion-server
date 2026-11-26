#include "aion/gameserver/model/templates/materials/MaterialTemplate.h"

#include "aion/gameserver/model/templates/materials/MaterialSkill.h"
#include "aion/gameserver/model/templates/materials/MaterialTargetInfo.h"

namespace aion::gameserver::model::templates::materials {

// Java MaterialTemplate.java:29-31
std::vector<const MaterialSkill*> MaterialTemplate::getSkills(gameobjects::Creature& creature) const {
	std::vector<const MaterialSkill*> result;
	for (const MaterialSkill& skill : skills) {
		if (matches(skill.getTarget(), creature))
			result.push_back(&skill);
	}
	return result;
}

} // namespace aion::gameserver::model::templates::materials
