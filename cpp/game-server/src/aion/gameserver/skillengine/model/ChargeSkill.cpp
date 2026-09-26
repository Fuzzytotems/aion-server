#include "aion/gameserver/skillengine/model/ChargeSkill.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::skillengine::model {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

ChargeSkill::ChargeSkill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLevelValue,
	int32_t motionIdValue, Skill& startSkill)
	// Java: super(skillTemplate, effector, skillLevel, startSkill.getFirstTarget(), null); this.motionId = motionId;
	: Skill(skillTemplateValue, effectorValue, skillLevelValue, startSkill.getFirstTarget(), nullptr), motionId(motionIdValue) {
	setClientHitTime(startSkill.getHitTime());
	setCastStartTime(startSkill.getCastStartTime());
	setCastSpeedForAnimationBoostAndChargeSkills(startSkill.getCastSpeedForAnimationBoostAndChargeSkills());
}

ChargeSkill::~ChargeSkill() = default;

runtime::Ref<ChargeSkill> ChargeSkill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue,
	int32_t skillLevelValue, int32_t motionIdValue, Skill& startSkill) {
	return runtime::makeRef<ChargeSkill>(skillTemplateValue, effectorValue, skillLevelValue, motionIdValue, startSkill);
}

bool ChargeSkill::useSkill() {
	if (!canUseSkill(properties::Properties_CastState::CAST_END)) {
		effector->getController().cancelCurrentSkill(nullptr);
		return false;
	}
	effector->getObserveController()->notifyBoostSkillCostObservers(*this);
	effector->getObserveController()->notifyStartSkillCastObservers(*this);
	effector->setCasting(Ptr<Skill>(this));
	effector->getObserveController()->attach(*moveListener);
	// motion boost state from the charge starting time must not get lost
	if (Ptr<Player> player = runtime::as<Player>(effector); player && player->isHitTimeBoosted(getCastStartTime()))
		player->setHitTimeBoost(commons::utils::currentTimeMillis() + 100, player->getHitTimeBoostCastSpeed());
	updateHitTime(configs::main::SecurityConfig::CHECK_ANIMATIONS);
	endCast();
	return true;
}

} // namespace aion::gameserver::skillengine::model
