#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * Attack status and damage calculations, and the target helpers used when a creature stops being a valid target.
 * <p>
 * C++: a static-only class. The attack result lists Java creates are returned as `std::vector<Ref<AttackResult>>` (the list holds the only
 * references, hub-headers.md §7.1); getWeaponGroup returns `std::optional` (Java returns null). The combat bodies are P5-01 work of M5b.
 *
 * @author ATracer
 */
class AttackUtil {
public:
	AttackUtil() = delete;

	/**
	 * This method calculates the physical attack status and damage in the following order: <br>
	 * 1. calculate status<br>
	 * 2. calculate main & off hand damage<br>
	 * 3. apply stat modifiers<br>
	 * 4. amplify damage by hit count<br>
	 */
	static std::vector<runtime::Ref<AttackResult>> calculatePhysAttackResult(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	static void adjustDamageByStatModifiers(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, AttackStatus status,
		const std::vector<runtime::Ptr<AttackResult>>& attackResultList, model::SkillElement element);

private:
	static std::vector<int32_t> calculateAdditionalHitCount(model::gameobjects::Creature& attacker, AttackStatus status,
		const std::vector<runtime::Ptr<AttackResult>>& attackList);

	static void amplifyDamageByAdditionalHitCount(model::gameobjects::Creature& attacker, AttackStatus status,
		const std::vector<runtime::Ptr<AttackResult>>& attackList);

	static void modifyDamageByNpcAi(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked,
		const std::vector<runtime::Ptr<AttackResult>>& attackStatus);

	static float calculateBlockedDamage(model::gameobjects::Creature& attacked, float damage);

	/** @param group nullable (getWeaponGroup) */
	static float calculateWeaponCritical(model::SkillElement element, model::gameobjects::Creature& attacked, float damage,
		std::optional<model::templates::item::enums::ItemGroup> group, int32_t critAddDmg, model::stats::container::StatEnum stat, bool isMain);

	/** @param group nullable (getWeaponGroup) */
	static float getWeaponMultiplier(std::optional<model::templates::item::enums::ItemGroup> group);

public:
	static void calculateSkillResult(skillengine::model::Effect& effect, int32_t skillDamage, const skillengine::effect::DamageEffect* template_,
		bool ignoreShield);

private:
	static float randomizeDamage(int32_t randomDamageType, float damage);

	static void calculateEffectResult(skillengine::model::Effect& effect, model::gameobjects::Creature& effected, int32_t damage, AttackStatus status,
		skillengine::model::HitType hitType, bool ignoreShield, int32_t position, bool send);

public:
	/**
	 * This method calculates the magical attack status and damage in the following order:<br>
	 * 1. calculate status<br>
	 * 2. calculate main & off hand damage<br>
	 * 3. apply stat modifiers<br>
	 * 4. amplify damage by hit count<br>
	 */
	static std::vector<runtime::Ref<AttackResult>> calculateMagAttackResult(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked,
		model::SkillElement element, const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	static int32_t calculateMagicalOverTimeSkillResult(skillengine::model::Effect& effect, float skillDamage,
		const skillengine::effect::EffectTemplate* template_, bool useMagicBoost);

private:
	static AttackStatus calculatePhysicalStatus(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked,
		const skillengine::effect::EffectTemplate* template_, skillengine::model::Effect& effect);

	static AttackStatus calculatePhysicalStatus(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, bool isMainHand,
		int32_t accMod, int32_t criticalProb, bool isSkill, bool cannotMiss);

public:
	/**
	 * Every + 100 delta of (MR - MA) = + 10% to resist<br>
	 * if the difference is 1000 = 100% resist
	 */
	static AttackStatus calculateMagicalStatus(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t criticalProb,
		bool isSkill);

	static void cancelCastOn(model::gameobjects::Creature& target);

	/**
	 * Send a packet to everyone who is targeting creature.
	 */
	static void removeTargetFrom(model::gameobjects::Creature& object);

	static void removeTargetFrom(model::gameobjects::Creature& object, bool validateSee);

private:
	/** @return null (std::nullopt) if the effector has no weapon of that hand */
	static std::optional<model::templates::item::enums::ItemGroup> getWeaponGroup(model::gameobjects::Creature& effector, bool mainHand);
};

} // namespace aion::gameserver::controllers::attack
