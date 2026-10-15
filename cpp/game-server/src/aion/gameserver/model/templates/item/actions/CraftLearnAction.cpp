#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/RecipeService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

// Java CraftLearnAction.java:26-35
void CraftLearnAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// Java dereferences parentItem first: a null is its NullPointerException (Ptr's operator->)
	if (!player.getInventory().decreaseByObjectId(parentItem->getObjectId(), 1))
		return;
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem->getL10n()));
	// client shows the "you learned" toast from this
	if (services::RecipeService::addRecipe(player, recipeid, false)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player.getObjectId(),
														 parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId()));
	}
}

// Java CraftLearnAction.java:38-40
bool CraftLearnAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	return services::RecipeService::validateNewRecipe(player, recipeid) != nullptr;
}

} // namespace aion::gameserver::model::templates::item::actions
