#include "aion/gameserver/services/abyss/AbyssSkillService.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/services/abyss/AbyssSkills.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::services::abyss {

using utils::stats::AbyssRankEnum;

/** Java AbyssSkills constructor data (race, rankenum, skills), in ordinal order; only this file uses the package-private enum */
struct AbyssSkillsData {
	model::Race race;
	AbyssRankEnum rankenum;
	std::initializer_list<int32_t> skills;
};

static const std::array<AbyssSkillsData, 10>& abyssSkillsData() {
	static const std::array<AbyssSkillsData, 10> data{{
		{model::Race::ELYOS, AbyssRankEnum::SUPREME_COMMANDER, {11889, 11898, 11900, 11903, 11904, 11905, 11906}},
		{model::Race::ELYOS, AbyssRankEnum::COMMANDER, {11888, 11898, 11900, 11903, 11904}},
		{model::Race::ELYOS, AbyssRankEnum::GREAT_GENERAL, {11887, 11897, 11899, 11903}},
		{model::Race::ELYOS, AbyssRankEnum::GENERAL, {11886, 11896, 11899}},
		{model::Race::ELYOS, AbyssRankEnum::STAR5_OFFICER, {11885, 11895}},
		{model::Race::ASMODIANS, AbyssRankEnum::SUPREME_COMMANDER, {11894, 11898, 11902, 11903, 11904, 11905, 11906}},
		{model::Race::ASMODIANS, AbyssRankEnum::COMMANDER, {11893, 11898, 11902, 11903, 11904}},
		{model::Race::ASMODIANS, AbyssRankEnum::GREAT_GENERAL, {11892, 11897, 11901, 11903}},
		{model::Race::ASMODIANS, AbyssRankEnum::GENERAL, {11891, 11896, 11901}},
		{model::Race::ASMODIANS, AbyssRankEnum::STAR5_OFFICER, {11890, 11895}},
	}};
	return data;
}

/** Java AbyssSkills.getSkills(Race, AbyssRankEnum) */
static std::span<const int32_t> getAbyssSkills(model::Race race, AbyssRankEnum rank) {
	for (const AbyssSkillsData& aSkills : abyssSkillsData()) {
		if (aSkills.race == race && aSkills.rankenum == rank) {
			return {aSkills.skills.begin(), aSkills.skills.size()};
		}
	}
	commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.abyss.AbyssSkills")
		.warn("No abyss skills for: " + std::string(xml::enumName(race)) + " " + std::string(xml::enumName(rank)));
	return {};
}

void AbyssSkillService::updateSkills(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::gameobjects::player::AbyssRank> abyssRank = player.getAbyssRank();
	if (!abyssRank) {
		return;
	}
	AbyssRankEnum rankEnum = abyssRank->getRank();
	// remove all abyss skills first
	for (const AbyssSkillsData& abyssSkill : abyssSkillsData()) {
		if (abyssSkill.race == player.getRace()) {
			for (int32_t skillId : abyssSkill.skills)
				SkillLearnService::removeSkill(player, skillId);
		}
	}
	// Java: RankingConfig.XFORM_MIN_RANK != null (the C++ config field always holds a rank); getId() is the ordinal + 1 for both enums
	if (static_cast<int32_t>(abyssRank->getRank()) >= static_cast<int32_t>(configs::main::RankingConfig::XFORM_MIN_RANK.load())) {
		// add new skills
		for (int32_t skillId : getAbyssSkills(player.getRace(), rankEnum))
			SkillLearnService::learnTemporarySkill(player, skillId, 1);
	}
}

} // namespace aion::gameserver::services::abyss
