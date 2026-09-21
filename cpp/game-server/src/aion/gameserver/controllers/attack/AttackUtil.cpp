#include "aion/gameserver/controllers/attack/AttackUtil.h"

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers::attack {

using model::gameobjects::Creature;
using runtime::Ptr;

std::vector<runtime::Ref<AttackResult>> AttackUtil::calculatePhysAttackResult(Creature& attacker, Creature& attacked,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

void AttackUtil::adjustDamageByStatModifiers(Creature& attacker, Creature& attacked, AttackStatus status, const std::vector<Ptr<AttackResult>>& attackResultList,
	model::SkillElement element) {
	AION_UNPORTED();
}

std::vector<int32_t> AttackUtil::calculateAdditionalHitCount(Creature& attacker, AttackStatus status, const std::vector<Ptr<AttackResult>>& attackList) {
	AION_UNPORTED();
}

void AttackUtil::amplifyDamageByAdditionalHitCount(Creature& attacker, AttackStatus status, const std::vector<Ptr<AttackResult>>& attackList) {
	AION_UNPORTED();
}

void AttackUtil::modifyDamageByNpcAi(Creature& attacker, Creature& attacked, const std::vector<Ptr<AttackResult>>& attackStatus) {
	AION_UNPORTED();
}

float AttackUtil::calculateBlockedDamage(Creature& attacked, float damage) {
	AION_UNPORTED();
}

float AttackUtil::calculateWeaponCritical(model::SkillElement element, Creature& attacked, float damage,
	std::optional<model::templates::item::enums::ItemGroup> group, int32_t critAddDmg, model::stats::container::StatEnum stat, bool isMain) {
	AION_UNPORTED();
}

float AttackUtil::getWeaponMultiplier(std::optional<model::templates::item::enums::ItemGroup> group) {
	AION_UNPORTED();
}

void AttackUtil::calculateSkillResult(skillengine::model::Effect& effect, int32_t skillDamage, const skillengine::effect::DamageEffect* template_,
	bool ignoreShield) {
	AION_UNPORTED();
}

float AttackUtil::randomizeDamage(int32_t randomDamageType, float damage) {
	AION_UNPORTED();
}

void AttackUtil::calculateEffectResult(skillengine::model::Effect& effect, Creature& effected, int32_t damage, AttackStatus status,
	skillengine::model::HitType hitType, bool ignoreShield, int32_t position, bool send) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<AttackResult>> AttackUtil::calculateMagAttackResult(Creature& attacker, Creature& attacked, model::SkillElement element,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t AttackUtil::calculateMagicalOverTimeSkillResult(skillengine::model::Effect& effect, float skillDamage,
	const skillengine::effect::EffectTemplate* template_, bool useMagicBoost) {
	AION_UNPORTED();
}

AttackStatus AttackUtil::calculatePhysicalStatus(Creature& attacker, Creature& attacked, const skillengine::effect::EffectTemplate* template_,
	skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

AttackStatus AttackUtil::calculatePhysicalStatus(Creature& attacker, Creature& attacked, bool isMainHand, int32_t accMod, int32_t criticalProb, bool isSkill,
	bool cannotMiss) {
	AION_UNPORTED();
}

AttackStatus AttackUtil::calculateMagicalStatus(Creature& attacker, Creature& attacked, int32_t criticalProb, bool isSkill) {
	AION_UNPORTED();
}

void AttackUtil::cancelCastOn(Creature& target) {
	target.getKnownList().forEachObject([&target](model::gameobjects::VisibleObject& visibleObject) {
		Ptr<Creature> creature = runtime::as<Creature>(visibleObject);
		if (creature && visibleObject.getTarget().get() == &target) {
			Ptr<skillengine::model::Skill> castingSkill = creature->getCastingSkill();
			if (castingSkill) {
				Ptr<Creature> firstTarget = castingSkill->getFirstTarget();
				if (!firstTarget) // Java: getFirstTarget().equals(target) on a null first target
					throw runtime::NullPointerException("casting skill has no first target");
				if (firstTarget->equals(target))
					creature->getController().cancelCurrentSkill(nullptr);
			}
		}
	});
}

void AttackUtil::removeTargetFrom(Creature& object) {
	removeTargetFrom(object, false);
}

void AttackUtil::removeTargetFrom(Creature& object, bool validateSee) {
	object.getKnownList().forEachPlayer([&object, validateSee](model::gameobjects::player::Player& player) {
		if (player.getTarget().get() == &object && (!validateSee || !player.canSee(Ptr<model::gameobjects::VisibleObject>(object))))
			player.setTarget(nullptr);
	});
}

std::optional<model::templates::item::enums::ItemGroup> AttackUtil::getWeaponGroup(Creature& effector, bool mainHand) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
