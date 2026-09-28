#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/itemgroups/fwd.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"

namespace aion::gameserver::services::reward {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author Rolandas, Pad, Neon
 */
class BonusService {
private:
	BonusService() = delete;
public:
	static const model::templates::quest::QuestItems* getQuestBonus(model::gameobjects::player::Player& player, const model::templates::QuestTemplate* questTemplate);
	static std::vector<const model::templates::itemgroups::ItemRaceEntry*> getMatchingItemsOfRandomGroup(model::gameobjects::player::Player& player, const model::templates::QuestTemplate* questTemplate);
private:
	static std::vector<const model::templates::itemgroups::BonusItemGroup*> getBonusGroups(model::templates::rewards::BonusType type);
};

} // namespace aion::gameserver::services::reward
