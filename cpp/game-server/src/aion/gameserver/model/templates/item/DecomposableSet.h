#pragma once

#include <vector>

#include "aion/gameserver/model/templates/item/DecomposableSet.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::templates::item {

/**
 * One reward branch of a decomposable item. Branches are rolled independently of each other, and a branch that fires yields exactly one of its
 * weighted alternatives (or nothing at all, if their chances don't add up to 100).
 * <p>
 * Java com.aionemu.gameserver.model.templates.item.DecomposableSet (upstream d254ea172). getItems returns the bound list, which is empty
 * when the data names no item (Java: Collections.emptyList()).
 */
class DecomposableSet : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/DecomposableSet.xml.inc"
public:
	const std::vector<DecomposedItem>& getItems() const { return items; }

	bool isApplicableTo(gameobjects::player::Player& player) const;

	bool roll() const;

	/**
	 * @return The one alternative this branch yields, or null if the alternatives don't cover the whole roll.
	 */
	const DecomposedItem* selectItem() const;
};

} // namespace aion::gameserver::model::templates::item
