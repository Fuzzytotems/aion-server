#include "aion/gameserver/model/stats/container/HomingGameStats.h"

#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::model::stats::container {

HomingGameStats::HomingGameStats(gameobjects::Npc& ownerValue) : SummonedObjectGameStats(ownerValue) {
}

HomingGameStats::~HomingGameStats() = default;

std::unique_ptr<calc::Stat2> HomingGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	std::unique_ptr<calc::Stat2> stat = SummonedObjectGameStats::getStat(statEnum, base, calculationTypes);
	runtime::Ptr<gameobjects::Creature> master = static_cast<gameobjects::Npc&>(owner).getMaster();
	if (master == nullptr)
		return stat;
	switch (statEnum) {
		case StatEnum::MAGICAL_ATTACK:
			stat->setBonusRate(0.2f);
			// Java returns the Stat2 that getItemStatBoost gives back, which is the argument itself
			master->getGameStats()->getItemStatBoost(statEnum, *stat);
			return stat;
		default:
			break;
	}
	return stat;
}

std::unique_ptr<calc::Stat2> HomingGameStats::getMainHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	gameobjects::Homing& homing = static_cast<gameobjects::Homing&>(static_cast<gameobjects::Npc&>(owner));
	const templates::stats::StatsTemplate* statsTemplate = getStatsTemplate();
	if (statsTemplate == nullptr) // Java: getStatsTemplate().getMagicalAttack() - NpcTemplate.statsTemplate is nullable
		throw runtime::NullPointerException("the homing has no stats template");
	int32_t power = statsTemplate->getMagicalAttack();
	const skillengine::model::SkillTemplate* skill = dataholders::DataManager::SKILL_DATA->getSkillTemplate(homing.getSkillId());
	if (skill == nullptr) // Java: skill.getLvl() on SKILL_DATA's null
		throw runtime::NullPointerException("Skill with ID " + std::to_string(homing.getSkillId()) + " does not exist");
	int32_t skillLvl = skill->getLvl();
	const std::string name = homing.getName();
	if (name == "gryphu")
		power = 324;
	switch (skillLvl) {
		case 3:
			if (name == "stone energy")
				power = 316;
			if (name == "water energy")
				power = 362;
			break;
		case 4:
			if (name == "cyclone servant")
				power = 1166;
			if (name == "fire energy")
				power = 313;
			if (name == "wind servant")
				power = 373;
			if (name == "stone energy")
				power = 384;
			break;
		case 5:
			if (name == "cyclone servant")
				power = 1221;
			break;
		case 6:
			if (name == "cyclone servant")
				power = 1283;
			break;
		case 7:
			if (name == "cyclone servant")
				power = 1342;
			break;
		default:
			break;
	}
	switch (homing.getLevel()) {
		case 65:
			if (name == "elemental energy")
				power = 1100;
			break;
		default:
			break;
	}
	return getStat(StatEnum::MAGICAL_ATTACK, static_cast<float>(power), calculationTypes);
}

} // namespace aion::gameserver::model::stats::container
