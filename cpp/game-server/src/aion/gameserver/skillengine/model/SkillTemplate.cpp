#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::model {

bool SkillTemplate::isPassive() const {
	AION_UNPORTED();
}

bool SkillTemplate::isToggle() const {
	AION_UNPORTED();
}

bool SkillTemplate::isProvoked() const {
	AION_UNPORTED();
}

bool SkillTemplate::isMaintain() const {
	AION_UNPORTED();
}

bool SkillTemplate::isActive() const {
	AION_UNPORTED();
}

bool SkillTemplate::isCharge() const {
	AION_UNPORTED();
}

const effect::EffectTemplate* SkillTemplate::getEffectTemplate(int32_t position) const {
	AION_UNPORTED();
}

bool SkillTemplate::hasAnyEffect(std::initializer_list<effect::EffectType> effectTypes) const {
	AION_UNPORTED();
}

bool SkillTemplate::hasAnyEffect(bool checkSubEffects, std::initializer_list<effect::EffectType> effectTypes) const {
	AION_UNPORTED();
}

bool SkillTemplate::hasResurrectEffect() const {
	AION_UNPORTED();
}

bool SkillTemplate::hasEvadeEffect() const {
	AION_UNPORTED();
}

bool SkillTemplate::hasRecallInstant() const {
	AION_UNPORTED();
}

int32_t SkillTemplate::getCooldownId() const {
	AION_UNPORTED();
}

bool SkillTemplate::isMultiCast() const {
	AION_UNPORTED();
}

const condition::ChainCondition* SkillTemplate::getChainCondition() const {
	AION_UNPORTED();
}

const condition::RideRobotCondition* SkillTemplate::getRideRobotCondition() const {
	AION_UNPORTED();
}

const condition::SkillChargeCondition* SkillTemplate::getSkillChargeCondition() const {
	AION_UNPORTED();
}

const condition::HpCondition* SkillTemplate::getHpCondition() const {
	AION_UNPORTED();
}

const condition::PlayerMovedCondition* SkillTemplate::getMovedCondition() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
