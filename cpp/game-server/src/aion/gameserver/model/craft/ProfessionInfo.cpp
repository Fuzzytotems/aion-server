#include "aion/gameserver/model/craft/ProfessionInfo.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::model::craft {

// The Profession methods of header request m5c-h04 (m5c-plan.md I-02), declared in ProfessionInfo.h; their bodies are C-01's (P5-09c).

std::optional<int32_t> getUpgradeCost(Profession profession, int32_t skillLevel) {
	switch (skillLevel) {
		case 0:
			return 3500;
		case 99:
			return 17000;
		case 199:
			return 115000;
		case 299:
			return 460000;
		case 449:
			// essence- and aethertapping have no artisan grade between expert and master
			return isCrafting(profession) ? std::optional<int32_t>(6004900) : std::nullopt;
		default:
			break;
	}
	return std::nullopt;
}

int32_t getMaxUpgradableLevel(Profession profession) {
	return isCrafting(profession) ? 499 : 399;
}

std::string getClientName(Profession profession) {
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(getSkillId(profession));
	if (skillTemplate == nullptr) // Java: NullPointerException at getSkillTemplate(skillId).getL10n()
		throw runtime::NullPointerException("SKILL_DATA.getSkillTemplate(" + std::to_string(getSkillId(profession)) + ") is null");
	return skillTemplate->getL10n();
}

std::string getClientName(Profession profession, int32_t skillLevel) {
	return getSkillGrade(profession, skillLevel) + " " + getClientName(profession);
}

std::string getSkillGrade(Profession profession, int32_t skillLevel) {
	if (skillLevel <= 99)
		return utils::ChatUtil::l10n(900797); // Amateur
	if (skillLevel <= 199)
		return utils::ChatUtil::l10n(900798); // Novice
	if (skillLevel <= 299)
		return utils::ChatUtil::l10n(900799); // Apprentice
	if (skillLevel <= 399)
		return utils::ChatUtil::l10n(900800); // Journeyman
	if (skillLevel <= 449)
		return utils::ChatUtil::l10n(900801); // Expert
	if (isCrafting(profession) && skillLevel <= 499)
		return utils::ChatUtil::l10n(902027); // Artisan
	return utils::ChatUtil::l10n(902028); // Master
}

std::optional<Profession> getBySkillId(int32_t skillId) {
	for (Profession profession : PROFESSION_VALUES) {
		if (getSkillId(profession) == skillId)
			return profession;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::craft
