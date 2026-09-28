#include "aion/gameserver/services/reward/BonusService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"

namespace aion::gameserver::services::reward {

std::optional<model::templates::quest::QuestItems> BonusService::getQuestBonus(model::gameobjects::player::Player& player, const model::templates::QuestTemplate* questTemplate) {
	AION_UNPORTED();
}

std::vector<const model::templates::itemgroups::ItemRaceEntry*> BonusService::getMatchingItemsOfRandomGroup(model::gameobjects::player::Player& player, const model::templates::QuestTemplate* questTemplate) {
	AION_UNPORTED();
}

std::vector<const model::templates::itemgroups::BonusItemGroup*> BonusService::getBonusGroups(model::templates::rewards::BonusType type) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
