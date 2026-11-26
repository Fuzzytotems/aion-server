#include "aion/gameserver/model/templates/item/DecomposableSet.h"

#include <algorithm>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::model::templates::item {

// Java DecomposableSet.java:40-43
bool DecomposableSet::isApplicableTo(gameobjects::player::Player& player) const {
	return (race == Race::PC_ALL || race == player.getRace()) &&
		(!playerClasses.has_value() || std::ranges::find(*playerClasses, player.getPlayerClass()) != playerClasses->end()) &&
		player.getLevel() >= minLevel && player.getLevel() <= maxLevel;
}

bool DecomposableSet::roll() const {
	return commons::utils::Rnd::get(1, 1000) <= utils::JavaMath::round(chance * 10);
}

// Java DecomposableSet.java:52-61
const DecomposedItem* DecomposableSet::selectItem() const {
	int32_t roll = commons::utils::Rnd::get(1, 10000);
	int32_t sum = 0;
	for (const DecomposedItem& item : getItems()) {
		sum += utils::JavaMath::round(item.getChance() * 100);
		if (roll <= sum)
			return &item;
	}
	return nullptr;
}

} // namespace aion::gameserver::model::templates::item
