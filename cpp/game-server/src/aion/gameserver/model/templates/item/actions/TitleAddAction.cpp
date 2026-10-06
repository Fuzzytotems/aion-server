#include "aion/gameserver/model/templates/item/actions/TitleAddAction.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

// Java TitleAddAction.java:28-39
bool TitleAddAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	if (titleid == 0 || parentItem == nullptr) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_ERROR());
		return false;
	}
	if (player.getTitleList().contains(titleid)) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_TITLE());
		return false;
	}
	return true;
}

// Java TitleAddAction.java:41-51
void TitleAddAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;
	const ItemTemplate* itemTemplate = parentItem->getItemTemplate();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), itemTemplate->getTemplateId()),
		true);
	// Java: ((int) (System.currentTimeMillis() / 1000)) + minutes * 60, int arithmetic
	if (player.getTitleList().addTitle(titleid, false,
			!minutes ? 0 : static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000)) +
													static_cast<uint32_t>(*minutes) * 60u))) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem->getL10n()));
		runtime::Ptr<gameobjects::Item> item = player.getInventory().getItemByObjId(parentItem->getObjectId());
		if (item == nullptr) // Java: Storage.delete(null)
			throw runtime::NullPointerException("Storage.delete(null)");
		player.getInventory().delete_(*item);
	}
}

} // namespace aion::gameserver::model::templates::item::actions
