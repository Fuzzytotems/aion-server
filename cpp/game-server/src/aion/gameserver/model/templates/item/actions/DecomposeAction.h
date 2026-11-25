#pragma once

#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.h"

#include <any>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.DecomposeAction.
 * <p>
 * C++ interim (upstream sync foundation, docs/deviations/P5-07.md): upstream d254ea172 reworked the decomposable templates (branches, weighted
 * alternatives, only_one) and this action. The rewards follow Java 68eccc7b1 (DecomposableItem.decompose, merged counts, the lore check, the
 * special cube check of canAct); the selection window, the use animation, the casting delay and the observer stay at 51d7a19cb
 * (SM_FIRST_SHOW_DECOMPOSABLE, SM_ITEM_USAGE_ANIMATION codes) until wave 5 ports d254ea172, cc61fb239 and 9b15b6df8. The static reward tables
 * and validateRandomItemIds of 51d7a19cb are gone with the random item types.
 *
 * @author oslo(a00441234)
 */
class DecomposeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

private:
	/** @param targetItem nullable: act's target item, which CM_USE_ITEM passes as null unless the client names one (CM_USE_ITEM.java:64-72) */
	bool postValidate(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem) const;

	/** @param targetItem nullable, as in postValidate */
	void finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		const DecomposableItem& decomposableItem) const;

	/**
	 * Branches that hit the same item stack their counts instead of granting it twice. (Java: a LinkedHashMap in first-hit order)
	 */
	std::vector<std::pair<int32_t, int64_t>> mergeRewards(const std::vector<const DecomposedItem*>& rewards) const;

	/**
	 * A limited possession item the player already owns voids the whole decompose, the parent item is not consumed.
	 */
	bool ownsLoreReward(gameobjects::player::Player& player, const std::vector<std::pair<int32_t, int64_t>>& rewards) const;

	bool containsSpecialCubeItems(const DecomposableItem& decomposableItem, gameobjects::player::Player& player) const;
};

} // namespace aion::gameserver::model::templates::item::actions
