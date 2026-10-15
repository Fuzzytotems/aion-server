#include "aion/gameserver/model/summons/SkillOrder.h"

#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::model::summons {

SkillOrder::SkillOrder(int32_t skillIdValue, int32_t skillLvlValue, gameobjects::Creature& targetValue, int32_t hateValue, bool releaseValue)
	: skillId(skillIdValue), skillLvl(skillLvlValue), target(targetValue), hate(hateValue), release_(releaseValue) {
}

SkillOrder::~SkillOrder() = default;

runtime::Ref<SkillOrder> SkillOrder::create(int32_t skillIdValue, int32_t skillLvlValue, gameobjects::Creature& targetValue, int32_t hateValue,
	bool releaseValue) {
	return runtime::makeRef<SkillOrder>(skillIdValue, skillLvlValue, targetValue, hateValue, releaseValue);
}

} // namespace aion::gameserver::model::summons
