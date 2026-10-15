#include "aion/gameserver/model/templates/rewards/CraftRecipe.h"

#include <algorithm>

#include "aion/gameserver/model/templates/QuestTemplate.h"

namespace aion::gameserver::model::templates::rewards {

bool CraftRecipe::matchesQuest(const QuestTemplate& questTemplate) const {
	if (!CraftReward::matchesQuest(questTemplate))
		return false;
	if (questTemplate.getCombineSkillPoint() < level)
		return false;
	if (questTemplate.getCombineSkillPoint() > getMaxLevel())
		return false;
	return true;
}

int32_t CraftRecipe::getMaxLevel() const {
	return std::min(level + 40, level / 100 * 100 + 99);
}

} // namespace aion::gameserver::model::templates::rewards
