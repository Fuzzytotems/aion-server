#pragma once

#include <vector>

#include "aion/gameserver/model/templates/item/DecomposableItem.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::templates::item {

/**
 * Java com.aionemu.gameserver.model.templates.item.DecomposableItem (upstream d254ea172). An absent set list is the empty bound vector (Java
 * replaces null with Collections.emptyList()); afterUnmarshal sorts the sets of an only_one item in place like Java's List.sort.
 *
 * @author antness
 */
class DecomposableItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/DecomposableItem.xml.inc"
public:
	/** @return the first applicable set with items, nullptr (Java null) if there is none */
	const DecomposableSet* getSelectableSet(gameobjects::player::Player& player) const;

	/**
	 * Rolls every applicable branch. Unless the item only ever yields one reward, every branch that fires contributes a reward.
	 */
	std::vector<const DecomposedItem*> decompose(gameobjects::player::Player& player) const;
};

} // namespace aion::gameserver::model::templates::item
