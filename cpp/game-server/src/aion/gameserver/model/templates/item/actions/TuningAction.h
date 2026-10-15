#pragma once

#include "aion/gameserver/model/templates/item/actions/TuningAction.xml.h"

#include <any>
#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.TuningAction. @author Rolandas */
class TuningAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/TuningAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	/**
	 * Java: DataManager.ITEM_RANDOM_BONUSES.selectRandomBonusNumber(StatBonusType.INVENTORY, item's statBonusSetId). Called by act's task, by
	 * ItemActionService.identifyItem (ItemActionService.java:45) and by ItemPurificationService (ItemPurificationService.java:130).
	 */
	static int32_t getRandomStatBonusIdFor(gameobjects::Item& item);
};

} // namespace aion::gameserver::model::templates::item::actions
