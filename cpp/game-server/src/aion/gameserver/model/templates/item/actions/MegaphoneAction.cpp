#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.h"

#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

// Java MegaphoneAction.java:26-29: Integer.parseInt(color, 16). C++: an absent color is the empty string, whose NumberFormatException
// ("For input string: \"\" under radix 16") stands for Java's "Cannot parse null string: null"
int32_t MegaphoneAction::getColor() const {
	int32_t rgb = commons::utils::parseInt(color, 16);
	return rgb;
}

// Java MegaphoneAction.java:31-34
bool MegaphoneAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*item*/, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	return true;
}

// Java MegaphoneAction.java:36-45. params[0] is the message (CM_MEGAPHONE)
void MegaphoneAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> item, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> params) const {
	using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
	using network::aion::serverpackets::SM_MEGAPHONE;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;
	const std::string message = std::any_cast<std::string>(params.begin()[0]); // Java: (String) params[0]
	const ItemTemplate* itemTemplate = item->getItemTemplate();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), item->getObjectId(), itemTemplate->getTemplateId()), true);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(item->getL10n()));
	player.getInventory().decreaseByObjectId(item->getObjectId(), 1);
	PacketSendUtility::broadcastToWorld(SM_MEGAPHONE(player, message, item->getItemId()));
}

} // namespace aion::gameserver::model::templates::item::actions
