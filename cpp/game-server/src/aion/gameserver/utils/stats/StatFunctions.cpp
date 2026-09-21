#include "aion/gameserver/utils/stats/StatFunctions.h"

#include <cmath>

#include "aion/gameserver/configs/main/FallDamageConfig.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/StatCapUtil.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::utils::stats {

using model::stats::container::StatEnum;

int64_t StatFunctions::calculateExperienceReward(int32_t maxLevelInRange, model::gameobjects::Npc& target) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateBaseExp(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateDPReward(model::gameobjects::player::Player& player, model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvEApGained(model::gameobjects::player::Player& player, model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvPApLost(model::gameobjects::player::Player& defeated, model::gameobjects::player::Player& winner) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvpApGained(model::gameobjects::player::Player& defeated, int32_t winnerAbyssRank, int32_t maxLevel) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvpXpGained(model::gameobjects::player::Player& defeated, int32_t winnerAbyssRank, int32_t maxLevel) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvpDpGained(model::gameobjects::player::Player& defeated, int32_t maxRank, int32_t maxLevel) {
	AION_UNPORTED();
}

int32_t StatFunctions::adjustPvpDpGained(int32_t points, int32_t defeatedLvl, int32_t killerLvl) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateHate(model::gameobjects::Creature& creature, int32_t value) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<controllers::attack::AttackResult>> StatFunctions::calculateAttackDamage(model::gameobjects::Creature& attacker,
	model::SkillElement element, controllers::attack::AttackStatus status, const std::unordered_set<CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

float StatFunctions::reduceDamageByElementalDefense(model::gameobjects::Creature& effector, model::gameobjects::Creature& attacked,
	model::SkillElement element, float damage) {
	AION_UNPORTED();
}

int32_t StatFunctions::getElementalDefenseDenominator(model::gameobjects::Creature& effector, model::gameobjects::Creature& attacked) {
	AION_UNPORTED();
}

float StatFunctions::calculateMagicalSkillDamage(model::gameobjects::Creature& effector, model::gameobjects::Creature& target, float baseDamage,
	int32_t bonus, const skillengine::effect::EffectTemplate* template_, bool useMagicBoost, bool useKnowledge, bool useBoostSpellAttack) {
	AION_UNPORTED();
}

bool StatFunctions::calculateMagicalCriticalRate(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t criticalProb) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateRatingMultiplier(model::templates::npc::NpcRating npcRating) {
	AION_UNPORTED();
}

int32_t StatFunctions::getApNpcRating(model::templates::npc::NpcRating npcRating) {
	AION_UNPORTED();
}

float StatFunctions::adjustDamageByPvpOrPveModifiers(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target, float baseDamage,
	int32_t pvpDamage, bool useTemplateDmg, model::SkillElement element) {
	AION_UNPORTED();
}

bool StatFunctions::checkIsDodgedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod) {
	AION_UNPORTED();
}

bool StatFunctions::checkIsParriedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod) {
	AION_UNPORTED();
}

bool StatFunctions::checkIsBlockedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod) {
	AION_UNPORTED();
}

bool StatFunctions::checkIsPhysicalCriticalHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, bool isMainHand,
	int32_t criticalProb, bool isSkill) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateMagicalResistRate(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod,
	model::SkillElement element) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateFallDamage(model::gameobjects::player::Player& player, float distance) {
	int32_t fallDamage = 0;
	if (distance >= configs::main::FallDamageConfig::MAXIMUM_DISTANCE_DAMAGE.load()) {
		fallDamage = player.getLifeStats()->getCurrentHp();
	} else if (distance >= configs::main::FallDamageConfig::MINIMUM_DISTANCE_DAMAGE.load()) {
		float dmgPerMeter = player.getLifeStats()->getMaxHp() * configs::main::FallDamageConfig::FALL_DAMAGE_PERCENTAGE.load() / 100.0f;
		fallDamage = model::templates::detail::floatToInt(distance * dmgPerMeter);
	}
	return fallDamage;
}

float StatFunctions::adjustStatByMovementModifier(model::gameobjects::Creature& creature, std::optional<StatEnum> stat, float value) {
	runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(creature);
	if (!player || !stat)
		return value;

	using Direction = controllers::movement::PlayableMoveController::MovementModifierDirection;
	// https://web.archive.org/web/20170429204823/gameguide.na.aiononline.com/aion/Combat
	switch (player->getMoveController()->getMovementDirection()) {
		case Direction::FORWARD:
			switch (*stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
					return value * 1.1f; // verified on 4.6 PTS
				case StatEnum::FIRE_RESISTANCE:
				case StatEnum::EARTH_RESISTANCE:
				case StatEnum::WATER_RESISTANCE:
				case StatEnum::WIND_RESISTANCE:
				case StatEnum::LIGHT_RESISTANCE:
				case StatEnum::DARK_RESISTANCE:
					return value - 50; // verified on 4.6 PTS
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return value * 0.8f; // verified on 4.6 PTS
				default:
					break;
			}
			break;
		case Direction::SIDEWAYS:
			switch (*stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
				case StatEnum::SPEED:
					return value * 0.8f; // verified on 4.6 PTS
				case StatEnum::EVASION:
					return value + 300;
				default:
					break;
			}
			break;
		case Direction::BACKWARD:
			switch (*stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
					return value * 0.8f; // verified on 4.6 PTS
				case StatEnum::SPEED:
					return value * 0.6f; // verified on 4.6 PTS
				case StatEnum::PARRY:
				case StatEnum::BLOCK:
					return value + 500;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return value;
}

float StatFunctions::getNpcLevelDiffMod(model::gameobjects::Creature& target, model::gameobjects::Creature& attacker) {
	int32_t levelDiff = target.getLevel() - attacker.getLevel();
	if (levelDiff <= 2)
		return 0.0f;
	if (levelDiff >= 12)
		return 1.0f;
	return (levelDiff - 2) * 0.1f;
}

float StatFunctions::limit(StatEnum statEnum, float value) {
	// Java: Math.min(int differenceLimit, float value) - the limit is an int, so only a NaN or a -0.0f value needs Java's special cases
	float differenceLimit = static_cast<float>(model::stats::calc::StatCapUtil::getDifferenceLimit(statEnum));
	if (differenceLimit == 0.0f && value == 0.0f && std::signbit(value))
		return value;
	return differenceLimit <= value ? differenceLimit : value;
}

} // namespace aion::gameserver::utils::stats
