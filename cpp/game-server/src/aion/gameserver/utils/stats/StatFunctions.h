#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/npc/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::utils::stats {

/**
 * Calculations are based on the following research:<br>
 * original: <a href="http://www.aionsource.com/forum/mechanic-analysis/42597-character-stats-xp-dp-origin-gerbator-team-july-2009-a.html">Link</a>
 * <br>
 * backup:
 * <a href="http://web.archive.org/web/20120111184941/http://www.aionsource.com/topic/40542-character-stats-xp-dp-origin-gerbatorteam-july-2009/">Link
 * </a>
 * <p>
 * C++: a static-only class. calculateAttackDamage returns the newly created results as `Ref` (hub-headers.md §7.1); adjustStatByMovementModifier
 * takes `std::optional<StatEnum>` (Java compares the stat with null).
 *
 * @author ATracer, alexa026, Neon
 */
class StatFunctions {
public:
	StatFunctions() = delete;

	static int64_t calculateExperienceReward(int32_t maxLevelInRange, model::gameobjects::Npc& target);

private:
	static int32_t calculateBaseExp(model::gameobjects::Npc& npc);

public:
	static int32_t calculateDPReward(model::gameobjects::player::Player& player, model::gameobjects::Creature& target);

	static int32_t calculatePvEApGained(model::gameobjects::player::Player& player, model::gameobjects::Creature& target);

	static int32_t calculatePvPApLost(model::gameobjects::player::Player& defeated, model::gameobjects::player::Player& winner);

	static int32_t calculatePvpApGained(model::gameobjects::player::Player& defeated, int32_t winnerAbyssRank, int32_t maxLevel);

	static int32_t calculatePvpXpGained(model::gameobjects::player::Player& defeated, int32_t winnerAbyssRank, int32_t maxLevel);

	static int32_t calculatePvpDpGained(model::gameobjects::player::Player& defeated, int32_t maxRank, int32_t maxLevel);

	static int32_t adjustPvpDpGained(int32_t points, int32_t defeatedLvl, int32_t killerLvl);

	/** Applies BOOST_HATE modifiers from equipment and buffs */
	static int32_t calculateHate(model::gameobjects::Creature& creature, int32_t value);

	static std::vector<runtime::Ref<controllers::attack::AttackResult>> calculateAttackDamage(model::gameobjects::Creature& attacker,
		model::SkillElement element, controllers::attack::AttackStatus status, const std::unordered_set<CalculationType>& calculationTypes);

private:
	/** Applies elemental defense to incoming damage. */
	static float reduceDamageByElementalDefense(model::gameobjects::Creature& effector, model::gameobjects::Creature& attacked,
		model::SkillElement element, float damage);

	static int32_t getElementalDefenseDenominator(model::gameobjects::Creature& effector, model::gameobjects::Creature& attacked);

public:
	static float calculateMagicalSkillDamage(model::gameobjects::Creature& effector, model::gameobjects::Creature& target, float baseDamage,
		int32_t bonus, const skillengine::effect::EffectTemplate* template_, bool useMagicBoost, bool useKnowledge, bool useBoostSpellAttack);

	/** Calculates MAGICAL CRITICAL chance */
	static bool calculateMagicalCriticalRate(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t criticalProb);

	static int32_t calculateRatingMultiplier(model::templates::npc::NpcRating npcRating);

	static int32_t getApNpcRating(model::templates::npc::NpcRating npcRating);

	static float adjustDamageByPvpOrPveModifiers(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target, float baseDamage,
		int32_t pvpDamage, bool useTemplateDmg, model::SkillElement element);

	/**
	 * Must be called only once since this method triggers observe controller checks with side effects (e.g. consume one always dodge effect
	 * activation)
	 */
	static bool checkIsDodgedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod);

	/**
	 * Must be called only once since this method triggers observe controller checks with side effects (e.g. consume one always parry effect
	 * activation)
	 */
	static bool checkIsParriedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod);

	/**
	 * Must be called only once since this method triggers observe controller checks with side effects (e.g. consume one always block effect
	 * activation)
	 */
	static bool checkIsBlockedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod);

	/**
	 * http://www.wolframalpha.com/input/?i=-0.000126341+x%5E2%2B0.184411+x-13.7738
	 * https://docs.google.com/spreadsheet/ccc?key=0AqxBGNJV9RrzdGNjbEhQNHN3S3M5bUVfUVQxRkVIT3c&hl=en_US#gid=0
	 * https://docs.google.com/spreadsheets/d/1QEET5QAnxqxgT2T82g80C_9yH2D5iFos2TadR_UqPQs/edit#gid=1008537650
	 */
	static bool checkIsPhysicalCriticalHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, bool isMainHand,
		int32_t criticalProb, bool isSkill);

	static int32_t calculateMagicalResistRate(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod,
		model::SkillElement element);

	static int32_t calculateFallDamage(model::gameobjects::player::Player& player, float distance);

	/** @param stat nullopt (Java null) returns the value unchanged */
	static float adjustStatByMovementModifier(model::gameobjects::Creature& creature, std::optional<model::stats::container::StatEnum> stat, float value);

private:
	static float getNpcLevelDiffMod(model::gameobjects::Creature& target, model::gameobjects::Creature& attacker);

public:
	/**
	 * @return the smaller of {@code value} and {@code differenceLimit} for this StatEnum
	 */
	static float limit(model::stats::container::StatEnum statEnum, float value);
};

} // namespace aion::gameserver::utils::stats
