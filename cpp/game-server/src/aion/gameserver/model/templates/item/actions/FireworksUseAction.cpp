#include "aion/gameserver/model/templates/item/actions/FireworksUseAction.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

// Java FireworksUseAction.java:21-24
bool FireworksUseAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	return true;
}

// Java FireworksUseAction.java:26-35
void FireworksUseAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;
	gameobjects::Item& parentItem = *parentItemPtr; // Java: a NullPointerException for no item
	if (parentItem.getActivationCount() > 1)
		parentItem.setActivationCount(parentItem.getActivationCount() - 1);
	else
		player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 1, 0), true);
	player.startCooldown(parentItem);
}

} // namespace aion::gameserver::model::templates::item::actions
