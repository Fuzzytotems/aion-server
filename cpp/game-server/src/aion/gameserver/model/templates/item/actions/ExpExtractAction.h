#pragma once

#include "aion/gameserver/model/templates/item/actions/ExpExtractAction.xml.h"

#include <any>
#include <initializer_list>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.ExpExtractAction. @author Rolandas, daddycaddy */
class ExpExtractAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/ExpExtractAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

private:
	/** Java private finishUse(Player, Item) (ExpExtractAction.java:78-95) */
	void finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem) const;
	/** Java private getRequiredExp(PlayerCommonData) (ExpExtractAction.java:97-102) */
	int64_t getRequiredExp(gameobjects::player::PlayerCommonData& cd) const;
};

} // namespace aion::gameserver::model::templates::item::actions
