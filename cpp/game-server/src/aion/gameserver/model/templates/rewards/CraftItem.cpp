#include "aion/gameserver/model/templates/rewards/CraftItem.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"

namespace aion::gameserver::model::templates::rewards {

int64_t CraftItem::getCount() const {
	return commons::utils::Rnd::get(3, 5);
}

bool CraftItem::matchesQuest(const QuestTemplate& questTemplate) const {
	if (!CraftReward::matchesQuest(questTemplate))
		return false;
	if (questTemplate.getCombineSkillPoint() < minLevel)
		return false;
	if (questTemplate.getCombineSkillPoint() > maxLevel)
		return false;
	return true;
}

} // namespace aion::gameserver::model::templates::rewards
