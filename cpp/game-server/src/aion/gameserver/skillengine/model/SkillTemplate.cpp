#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include <cstddef>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/condition/ChainCondition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/condition/HpCondition.h"
#include "aion/gameserver/skillengine/condition/PlayerMovedCondition.h"
#include "aion/gameserver/skillengine/condition/RideRobotCondition.h"
#include "aion/gameserver/skillengine/condition/SkillChargeCondition.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/effect/SubEffect.h"

namespace aion::gameserver::skillengine::model {

namespace {

/** Java `for (Condition cond : conditions.getConditions()) if (cond instanceof C) return (C) cond;` */
template <class C>
const C* findCondition(const condition::Conditions& conditions) {
	for (const std::unique_ptr<condition::Condition>& cond : conditions.getConditions()) {
		if (const C* found = dynamic_cast<const C*>(cond.get()))
			return found;
	}
	return nullptr;
}

/** Java DataManager.SKILL_DATA.getSkillTemplate(skillId) */
const SkillTemplate* skillTemplateById(int32_t skillId) {
	return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
}

} // namespace

bool SkillTemplate::isPassive() const {
	return activationAttribute == ActivationAttribute::PASSIVE;
}

bool SkillTemplate::isToggle() const {
	return activationAttribute == ActivationAttribute::TOGGLE;
}

bool SkillTemplate::isProvoked() const {
	return activationAttribute == ActivationAttribute::PROVOKED;
}

bool SkillTemplate::isMaintain() const {
	return activationAttribute == ActivationAttribute::MAINTAIN;
}

bool SkillTemplate::isActive() const {
	return activationAttribute == ActivationAttribute::ACTIVE;
}

bool SkillTemplate::isCharge() const {
	return activationAttribute == ActivationAttribute::CHARGE;
}

const effect::EffectTemplate* SkillTemplate::getEffectTemplate(int32_t position) const {
	if (!effects || static_cast<int64_t>(effects->getEffects().size()) < position)
		return nullptr;
	if (position < 1) // Java: List.get(position - 1)
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(static_cast<int64_t>(position) - 1) + " out of bounds for length " +
			std::to_string(effects->getEffects().size()));
	return effects->getEffects()[static_cast<size_t>(position - 1)].get();
}

bool SkillTemplate::hasAnyEffect(std::initializer_list<effect::EffectType> effectTypes) const {
	return hasAnyEffect(false, effectTypes);
}

bool SkillTemplate::hasAnyEffect(bool checkSubEffects, std::initializer_list<effect::EffectType> effectTypes) const {
	if (!effects)
		return false;
	if (effects->hasAnyEffectType(effectTypes))
		return true;
	if (checkSubEffects) {
		for (const std::unique_ptr<effect::EffectTemplate>& et : effects->getEffects()) {
			if (et->getSubEffect() != nullptr) {
				const SkillTemplate* subSkill = skillTemplateById(et->getSubEffect()->getSkillId());
				if (subSkill == nullptr) // Java: getSkillTemplate(...).hasAnyEffect(...) on null
					throw runtime::NullPointerException("no skill template " + std::to_string(et->getSubEffect()->getSkillId()) + " (sub effect of skill " +
						std::to_string(skillId) + ")");
				if (subSkill->hasAnyEffect(effectTypes)) // should we check recursively?
					return true;
			}
		}
	}
	return false;
}

bool SkillTemplate::hasResurrectEffect() const {
	return hasAnyEffect({effect::EffectType::RESURRECT, effect::EffectType::RESURRECTPOSITIONAL});
}

bool SkillTemplate::hasEvadeEffect() const {
	return hasAnyEffect({effect::EffectType::EVADE});
}

bool SkillTemplate::hasRecallInstant() const {
	return hasAnyEffect({effect::EffectType::RECALLINSTANT});
}

int32_t SkillTemplate::getCooldownId() const {
	return (cooldownId > 0) ? cooldownId : skillId;
}

bool SkillTemplate::isMultiCast() const {
	const condition::ChainCondition* chainCondition = getChainCondition();
	return chainCondition != nullptr && chainCondition->getAllowedActivations() > 1;
}

const condition::ChainCondition* SkillTemplate::getChainCondition() const {
	return startconditions ? findCondition<condition::ChainCondition>(*startconditions) : nullptr;
}

const condition::RideRobotCondition* SkillTemplate::getRideRobotCondition() const {
	return useconditions ? findCondition<condition::RideRobotCondition>(*useconditions) : nullptr;
}

const condition::SkillChargeCondition* SkillTemplate::getSkillChargeCondition() const {
	return startconditions ? findCondition<condition::SkillChargeCondition>(*startconditions) : nullptr;
}

const condition::HpCondition* SkillTemplate::getHpCondition() const {
	if (!startconditions) // Java iterates startconditions.getConditions() without a null check
		throw runtime::NullPointerException("SkillTemplate.startconditions is null (skill " + std::to_string(skillId) + ")");
	return findCondition<condition::HpCondition>(*startconditions);
}

const condition::PlayerMovedCondition* SkillTemplate::getMovedCondition() const {
	if (!startconditions) // Java iterates startconditions.getConditions() without a null check
		throw runtime::NullPointerException("SkillTemplate.startconditions is null (skill " + std::to_string(skillId) + ")");
	return findCondition<condition::PlayerMovedCondition>(*startconditions);
}

} // namespace aion::gameserver::skillengine::model
