#include "aion/gameserver/model/templates/rewards/CraftReward.h"

#include "aion/gameserver/model/templates/QuestTemplate.h"

namespace aion::gameserver::model::templates::rewards {

bool CraftReward::matchesQuest(const QuestTemplate& questTemplate) const {
	return questTemplate.getCombineSkill() == skill;
}

} // namespace aion::gameserver::model::templates::rewards
