#include "aion/gameserver/skillengine/effect/ProvokerEffect.h"

#include <cstdint>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HitType.h"
#include "aion/gameserver/skillengine/model/ProvokeTarget.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::effect {

using controllers::observer::ObserverType;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java's implicit null check of a dereference of a static template pointer (a plain C++ dereference of nullptr is undefined) */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

} // namespace

/**
 * Java: the anonymous ActionObserver of ProvokerEffect.startEffect (ProvokerEffect.java:44-65, fieldmap key ProvokerEffect$1). Stored in the
 * effected creature's ObserveController and, through the removal task of Effect.addObserver, in the effect's observerRemoveTasks;
 * Effect.endEffect -> removeObservers removes it from both (cycles.toml "ProvokerEffect$1#effector": java-hook). The template is immortal static
 * data (fieldmap: captured this: final template reference); the observer reads its private shouldApply and getProvokeTarget and its protected
 * skillId, as Java's inner class does (the friend declaration of ProvokerEffect.h).
 */
struct ProvokerEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const ProvokerEffect* provokerEffect; // captured this ProvokerEffect this (line 57)
	const Ref<Creature> effector; // captured local Creature effector (line 48)

	static Ref<ProvokerEffect_ActionObserver> create(const ProvokerEffect& provokerEffect, ObserverType observerType, Creature& effector) {
		return runtime::makeRef<ProvokerEffect_ActionObserver>(provokerEffect, observerType, effector);
	}

	void attack(Creature& attacked, int32_t attackSkillId) override { tryApplyEffect(attacked, attackSkillId, *effector); }

	void attacked(Creature& attacker, int32_t attackSkillId) override { tryApplyEffect(attacker, attackSkillId, *effector); }

private:
	void tryApplyEffect(Creature& target, int32_t attackSkillId, Creature& effectorValue) {
		if (provokerEffect->shouldApply(effectorValue, target, attackSkillId)) {
			if (Ptr<Player> player = runtime::as<Player>(effectorValue)) {
				utils::PacketSendUtility::sendPacket(*player,
					network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_SKILL_PROC_EFFECT_OCCURRED(
						nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(provokerEffect->skillId), "SKILL_DATA.getSkillTemplate(skillId)")
							.getL10n()));
			}
			SkillEngine::getInstance().applyEffectDirectly(provokerEffect->skillId, effectorValue,
				*provokerEffect->getProvokeTarget(effectorValue, target));
		}
	}

protected:
	ProvokerEffect_ActionObserver(const ProvokerEffect& provokerEffectValue, ObserverType observerType, Creature& effectorValue)
		: ActionObserver(observerType), provokerEffect(&provokerEffectValue), effector(Ref<Creature>(effectorValue)) {}
	~ProvokerEffect_ActionObserver() override = default;
};

void ProvokerEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void ProvokerEffect::startEffect(model::Effect& effect) const {
	Ptr<Creature> effector = effect.getEffector();
	ObserverType observerType = hitType == model::HitType::NMLATK || hitType == model::HitType::BACKATK ? ObserverType::ATTACK : ObserverType::ATTACKED;
	Ref<ProvokerEffect_ActionObserver> observer = ProvokerEffect_ActionObserver::create(*this, observerType, *effector);
	effect.addObserver(*effect.getEffected(), *observer);
}

bool ProvokerEffect::shouldApply(gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& target,
	int32_t attackSkillId) const {
	if (provokeTarget == model::ProvokeTarget::OPPONENT && &target == &effector)
		return false;
	if (radius > 0 && !utils::PositionUtil::isInRange(effector, target, static_cast<float>(radius), false))
		return false;
	if (commons::utils::Rnd::chance() >= static_cast<float>(hitTypeProb))
		return false;
	switch (hitType) {
		case model::HitType::PHHIT:
			return attackSkillId == 0
				|| nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(attackSkillId), "SKILL_DATA.getSkillTemplate(attackSkillId)").getType()
				== model::SkillType::PHYSICAL;
		case model::HitType::MAHIT:
			return attackSkillId != 0
				&& nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(attackSkillId), "SKILL_DATA.getSkillTemplate(attackSkillId)").getType()
				== model::SkillType::MAGICAL;
		case model::HitType::BACKATK:
			return utils::PositionUtil::isBehind(effector, target);
		default:
			return true;
	}
}

runtime::Ptr<gameserver::model::gameobjects::Creature> ProvokerEffect::getProvokeTarget(gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& target) const {
	// Java: a switch expression over the nullable field, which throws NullPointerException for null
	if (!provokeTarget)
		throw runtime::NullPointerException("Cannot invoke \"ProvokeTarget.ordinal()\" because \"this.provokeTarget\" is null");
	switch (*provokeTarget) {
		case model::ProvokeTarget::ME:
			return Ptr<Creature>(effector);
		case model::ProvokeTarget::OPPONENT:
			return Ptr<Creature>(target);
	}
	// Java: the exhaustive switch expression throws MatchException for a constant it does not know; C++ reaches this only with a value outside
	// the enum
	throw commons::utils::IllegalStateException("Unknown ProvokeTarget " + std::to_string(static_cast<int32_t>(*provokeTarget)));
}

void ProvokerEffect::endEffect(model::Effect& /*effect*/) const {
	// Java: empty body
}

} // namespace aion::gameserver::skillengine::effect
