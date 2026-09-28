#include "aion/gameserver/skillengine/properties/TargetStatusProperty.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::Creature;
using runtime::Ptr;

bool TargetStatusProperty::set(const Properties* properties, Properties::ValidationResult& result, const model::SkillTemplate* skillTemplate) {
	// TODO find out why skill 2504-2506 ("Protective Shell") has target_status="STUN STAGGER STUMBLE SPIN OPENAERIAL"
	if (skillTemplate->getStack() == "RI_PROTECTIONCURTAIN")
		return true;

	// Java: properties.getTargetStatus(); Properties.validateEffectedList runs this step only when target_status is set
	const std::vector<effect::AbnormalState>& states = *properties->getTargetStatus();
	result.getTargets().removeIf([&states](const Ptr<Creature>& effected) { return !hasAnyAbnormalState(*effected, states); });

	// if first target was filtered out (= he had no required abnormal state), the skill cannot be cast
	return result.getTargets().contains(result.getFirstTarget());
}

bool TargetStatusProperty::hasAnyAbnormalState(Creature& creature, const std::vector<effect::AbnormalState>& states) {
	for (effect::AbnormalState state : states) {
		if (creature.getEffectController()->isAbnormalSet(state))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::skillengine::properties
