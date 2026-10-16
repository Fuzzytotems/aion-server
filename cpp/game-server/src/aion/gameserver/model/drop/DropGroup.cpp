#include "aion/gameserver/model/drop/DropGroup.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::model::drop {

int32_t DropGroup::tryAddDropItems(runtime::RcHashSet<runtime::Ref<DropItem>>& result, int32_t index, DropModifiers& dropModifiers,
	const std::vector<runtime::Ptr<gameobjects::player::Player>>& groupMembers) const {
	// Deviation: Java copies the drops into a HashSet of identity-hashed Drops (an arbitrary order that changes between runs); the C++ list keeps
	// the XML order. Ties between drops of the same chance are chosen at random either way (docs/deviations/P4-13.md)
	std::vector<const Drop*> remainingDrops;
	remainingDrops.reserve(drops.size());
	for (const std::unique_ptr<Drop>& drop : drops)
		remainingDrops.push_back(drop.get());
	for (int32_t i = 0; i < maxItems && !remainingDrops.empty(); i++) {
		float chance = commons::utils::Rnd::chance();
		float nearestChanceDiff = std::numeric_limits<float>::max();
		std::vector<const Drop*> nearestDropsOfSameChance;
		for (const Drop* drop : remainingDrops) {
			float finalChance = dropModifiers.calculateDropChance(drop->getChance(), isUseLevelBasedChanceReduction());
			if (chance < finalChance) {
				float chanceDiff = finalChance - chance;
				if (nearestDropsOfSameChance.empty() || chanceDiff <= nearestChanceDiff) {
					if (chanceDiff < nearestChanceDiff) {
						nearestDropsOfSameChance.clear();
						nearestChanceDiff = chanceDiff;
					}
					nearestDropsOfSameChance.push_back(drop);
				}
			}
		}
		const Drop* const* drop = commons::utils::Rnd::get(nearestDropsOfSameChance);
		if (drop != nullptr) {
			const Drop* chosen = *drop;
			index = addDropItem(index, result, *chosen, groupMembers);
			remainingDrops.erase(std::find(remainingDrops.begin(), remainingDrops.end(), chosen));
		}
	}
	return index;
}

int32_t DropGroup::addDropItem(int32_t index, runtime::RcHashSet<runtime::Ref<DropItem>>& result, const Drop& drop,
	const std::vector<runtime::Ptr<gameobjects::player::Player>>& groupMembers) const {
	if (drop.isEachMember() && !groupMembers.empty()) {
		for (runtime::Ptr<gameobjects::player::Player> player : groupMembers) {
			runtime::Ref<DropItem> dropitem = DropItem::create(&drop);
			dropitem->calculateCount();
			dropitem->setIndex(index++);
			dropitem->setPlayerObjId(player->getObjectId());
			dropitem->setWinningPlayer(player);
			dropitem->isDistributeItem(true);
			result.add(dropitem);
		}
	} else {
		runtime::Ref<DropItem> dropitem = DropItem::create(&drop);
		dropitem->calculateCount();
		dropitem->setIndex(index++);
		result.add(dropitem);
	}
	return index;
}

} // namespace aion::gameserver::model::drop
