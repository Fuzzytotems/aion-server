#include "aion/gameserver/controllers/observer/AttackShieldObserver.h"

#include <algorithm>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/SubEffect.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::controllers::observer {

using attack::AttackResult;
using attack::AttackStatus;
using model::gameobjects::Creature;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ptr;
using skillengine::model::HealType;
using skillengine::model::HitType;
using skillengine::model::ShieldType;
using utils::PositionUtil;

AttackShieldObserver::AttackShieldObserver(int32_t hitValue, int32_t totalHitValue, bool percent, skillengine::model::Effect& effectValue,
	HitType type, ShieldType shieldTypeValue, int32_t probabilityValue)
	: AttackShieldObserver(hitValue, totalHitValue, percent, false, effectValue, type, shieldTypeValue, probabilityValue, 0, 100, std::nullopt, 0) {
}

AttackShieldObserver::AttackShieldObserver(int32_t hitValue, int32_t totalHitValue, bool percent, skillengine::model::Effect& effectValue,
	HitType type, ShieldType shieldTypeValue, int32_t probabilityValue, int32_t mpValueValue)
	: AttackShieldObserver(hitValue, totalHitValue, percent, false, effectValue, type, shieldTypeValue, probabilityValue, 0, 100, std::nullopt,
		  mpValueValue) {
}

AttackShieldObserver::AttackShieldObserver(int32_t hitValue, int32_t totalHitValue, bool hitPercentValue, bool totalHitPercentValue,
	skillengine::model::Effect& effectValue, HitType type, ShieldType shieldTypeValue, int32_t probabilityValue, int32_t minRadiusValue,
	int32_t maxRadiusValue, std::optional<HealType> healTypeValue, int32_t mpValueValue)
	: effect(effectValue), hitType(type), shieldType(shieldTypeValue), hit(hitValue), hitPercent(hitPercentValue),
	  totalHit(totalHitValue), // total absorbed dmg for shield, percentage for reflector, received dmg percentage for protect
	  totalHitPercent(totalHitPercentValue), probability(probabilityValue),
	  minRadius(minRadiusValue), // only for reflector
	  maxRadius(maxRadiusValue), // for reflector / protect
	  healType(healTypeValue),   // only for ConvertHeal
	  mpValue(mpValueValue) {
}

AttackShieldObserver::~AttackShieldObserver() = default;

runtime::Ref<AttackShieldObserver> AttackShieldObserver::create(int32_t hitValue, int32_t totalHitValue, bool percent,
	skillengine::model::Effect& effectValue, HitType type, ShieldType shieldTypeValue, int32_t probabilityValue) {
	return runtime::makeRef<AttackShieldObserver>(hitValue, totalHitValue, percent, effectValue, type, shieldTypeValue, probabilityValue);
}

runtime::Ref<AttackShieldObserver> AttackShieldObserver::create(int32_t hitValue, int32_t totalHitValue, bool percent,
	skillengine::model::Effect& effectValue, HitType type, ShieldType shieldTypeValue, int32_t probabilityValue, int32_t mpValueValue) {
	return runtime::makeRef<AttackShieldObserver>(hitValue, totalHitValue, percent, effectValue, type, shieldTypeValue, probabilityValue,
		mpValueValue);
}

runtime::Ref<AttackShieldObserver> AttackShieldObserver::create(int32_t hitValue, int32_t totalHitValue, bool hitPercentValue,
	bool totalHitPercentValue, skillengine::model::Effect& effectValue, HitType type, ShieldType shieldTypeValue, int32_t probabilityValue,
	int32_t minRadiusValue, int32_t maxRadiusValue, std::optional<HealType> healTypeValue, int32_t mpValueValue) {
	return runtime::makeRef<AttackShieldObserver>(hitValue, totalHitValue, hitPercentValue, totalHitPercentValue, effectValue, type,
		shieldTypeValue, probabilityValue, minRadiusValue, maxRadiusValue, healTypeValue, mpValueValue);
}

void AttackShieldObserver::checkShield(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList,
	runtime::Ptr<skillengine::model::Effect> attackerEffect, model::gameobjects::Creature& attacker) {
	for (const Ptr<AttackResult>& attackResult : attackList) {
		AttackStatus baseStatus = controllers::detail::getBaseStatus(attackResult->getAttackStatus());
		if (baseStatus == AttackStatus::DODGE || baseStatus == AttackStatus::RESIST)
			continue;

		// Handle Hit Types for Shields
		switch (hitType) {
			case HitType::EVERYHIT:
				break;
			case HitType::SKILL:
				if (!attackerEffect)
					continue;
				break;
			default:
				// Java: attackResult.getHitType() != null && hitType != attackResult.getHitType(); the C++ AttackResult has no null hit type
				if (hitType != attackResult->getHitType())
					continue;
		}

		if (probability < 100 && commons::utils::Rnd::chance() >= probability)
			continue;

		// shield type 2 or 16, normal shield, MP
		if (shieldType == ShieldType::NORMAL || shieldType == ShieldType::MPSHIELD) {
			int32_t damage = attackResult->getDamage();

			int32_t absorbedDamage;
			if (hitPercent)
				absorbedDamage = controllers::detail::mul(damage, hit) / 100;
			else
				absorbedDamage = std::min(damage, hit);

			absorbedDamage = std::min(absorbedDamage, totalHit.get());
			totalHit = controllers::detail::sub(totalHit.get(), absorbedDamage);

			if (absorbedDamage > 0)
				attackResult->setShieldType(detail::shieldTypeId(shieldType));
			attackResult->setDamage(static_cast<float>(controllers::detail::sub(damage, absorbedDamage)));

			// don't launch sub effect if damage is fully absorbed
			if (absorbedDamage >= damage && !isPunchShield(attackerEffect))
				attackResult->setLaunchSubEffect(false);

			if (mpValue != 0) {
				int32_t mp = controllers::detail::toInt(absorbedDamage * 0.01f * mpValue);
				// TODO recheck sm_attack_status
				effect->getEffected()->getLifeStats()->reduceMp(SM_ATTACK_STATUS_TYPE::USED_MP, mp, 0, SM_ATTACK_STATUS_LOG::REGULAR);
				attackResult->setMpAbsorbed(mp);
				attackResult->setMpShieldSkillId(effect->getSkillId());
			}

			if (totalHit.get() <= 0) {
				effect->endEffect();
				return;
			}
		} else if (shieldType == ShieldType::REFLECTOR || shieldType == ShieldType::SKILL_REFLECTOR) { // shield type 1, reflected damage
			if (minRadius != 0) {
				if (PositionUtil::isInRange(attacker, *effect->getEffected(), static_cast<float>(minRadius), false))
					continue;
			}
			if (PositionUtil::isInRange(attacker, *effect->getEffected(), static_cast<float>(maxRadius), false)) {
				int32_t reflectedHit = attackResult->getDamage();
				if (hit > 0 || totalHit.get() > 0) {
					int32_t reflectedDamage = controllers::detail::mul(attackResult->getDamage(), totalHit.get()) / 100;
					reflectedHit = std::max(reflectedDamage, hit); // percentage of damage, but at least hit value
				}
				attackResult->setShieldType(detail::shieldTypeId(shieldType));
				if (runtime::as<model::gameobjects::Npc>(attacker)) {
					reflectedHit = controllers::detail::toInt(attacker.getAi().modifyDamage(attacker, static_cast<float>(reflectedHit), effect));
				}
				attackResult->setReflectedDamage(reflectedHit);
				attackResult->setReflectedSkillId(effect->getSkillId());

				if (shieldType == ShieldType::SKILL_REFLECTOR) { // whole skill reflections are applied implicitly, see Effect#getEffected()
					attackerEffect->setForceType(skillengine::model::Effect_ForceType::DEFAULT); // make sure it hits the effector (no checks needed at this point)
					effect->endEffect(); // one skill reflection ends the shield effect
					return;
				} else { // apply reflect damage
					attacker.getController().onAttack(*effect->getEffected(), effect, SM_ATTACK_STATUS_TYPE::REGULAR, reflectedHit, false,
						SM_ATTACK_STATUS_LOG::REGULAR, std::nullopt, std::nullopt);
				}
			}
			break;
		} else if (shieldType == ShieldType::PROTECT) { // shield type 8, protect effect (ex. skillId: 417 Bodyguard I)
			if (!effect->getEffector() || effect->getEffector()->isDead()) {
				effect->endEffect();
				break;
			}
			if (Ptr<model::gameobjects::Summon> summon = runtime::as<model::gameobjects::Summon>(effect->getEffector());
				summon && (summon->getMode() == model::summons::SummonMode::RELEASE || !summon->getMaster())) {
				effect->endEffect();
				break;
			}

			if (PositionUtil::isInRange(*effect->getEffector(), *effect->getEffected(), static_cast<float>(maxRadius), false)) {
				int32_t damageProtected = 0;
				int32_t effectorDamage = 0;

				if (hitPercent) {
					damageProtected = controllers::detail::toInt(attackResult->getDamage() * hit * 0.01);
					if (totalHit.get() > 0) // reduce the effectively received damage (totalHit = percent of received dmg)
						effectorDamage = controllers::detail::mul(attackResult->getDamage(), totalHit.get()) / 100;
					else
						effectorDamage = attackResult->getDamage();
				} else
					damageProtected = hit;
				int32_t finalDamage = std::max(0, controllers::detail::sub(attackResult->getDamage(), damageProtected));
				attackResult->setDamage(static_cast<float>(finalDamage));
				attackResult->setShieldType(detail::shieldTypeId(shieldType));
				attackResult->setProtectedSkillId(effect->getSkillId());
				attackResult->setProtectedDamage(effectorDamage);
				attackResult->setProtectorId(effect->getEffectorId());
				effect->getEffector()->getController().onAttack(attacker, attackerEffect, SM_ATTACK_STATUS_TYPE::PROTECTDMG, effectorDamage, false,
					SM_ATTACK_STATUS_LOG::REGULAR, attackResult->getAttackStatus(), std::nullopt);
				// dont launch subeffect if damage is fully absorbed
				if (!isPunchShield(attackerEffect))
					attackResult->setLaunchSubEffect(false);
			}
		} else if (shieldType == ShieldType::CONVERT) { // shield type 0, convertHeal
			int32_t damage = attackResult->getDamage();

			int32_t absorbedDamage = damage;

			if (totalHitPercent && !totalHitPercentSet.get()) {
				totalHit = controllers::detail::toInt(totalHit.get() * 0.01 * effect->getEffected()->getGameStats()->getHealth()->getCurrent());
				totalHitPercentSet = true;
			}

			absorbedDamage = std::min(absorbedDamage, totalHit.get());
			totalHit = controllers::detail::sub(totalHit.get(), absorbedDamage);

			attackResult->setDamage(static_cast<float>(controllers::detail::sub(damage, absorbedDamage)));

			// heal part
			int32_t healValue = 0;
			if (hitPercent)
				healValue = controllers::detail::mul(damage, hit) / 100;
			else
				healValue = hit;

			switch (detail::unbox(healType, "healType")) { // Java: a switch on a null healType throws NullPointerException
				case HealType::HP:
					effect->getEffected()->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, *effect, SM_ATTACK_STATUS_LOG::REGULAR);
					break;
				case HealType::MP:
					effect->getEffected()->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::HEAL_MP, healValue, effect->getSkillId(),
						SM_ATTACK_STATUS_LOG::REGULAR);
					break;
				default:
					break;
			}

			// dont launch subeffect if damage is fully absorbed
			if (absorbedDamage >= damage && !isPunchShield(attackerEffect))
				attackResult->setLaunchSubEffect(false);

			if (totalHit.get() <= 0) {
				effect->endEffect();
				return;
			}
		}
	}
}

bool AttackShieldObserver::isPunchShield(runtime::Ptr<skillengine::model::Effect> value) {
	if (!value)
		return false;
	for (const skillengine::effect::EffectTemplate* template_ : value->getEffectTemplates()) {
		if (template_->getSubEffect() != nullptr) {
			const skillengine::model::SkillTemplate* skill = detail::nonNull(
				dataholders::DataManager::SKILL_DATA->getSkillTemplate(template_->getSubEffect()->getSkillId()), "SKILL_DATA.getSkillTemplate(skillId)");
			if (skill->isProvoked())
				return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::controllers::observer
