#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.h"

#include <algorithm>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"


namespace aion::gameserver::model::templates::item::actions {

// Java EmotionLearnAction.java:39-50
bool EmotionLearnAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	if (emotionId == 0 || parentItem == nullptr) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_ERROR());
		return false;
	}
	if (player.getEmotions() != nullptr && player.getEmotions()->contains(emotionId)) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_EMOTION());
		return false;
	}
	return true;
}

// Java EmotionLearnAction.java:52-60
void EmotionLearnAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;
	const ItemTemplate* itemTemplate = parentItem->getItemTemplate();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), itemTemplate->getTemplateId()),
		true);
	runtime::Ptr<gameobjects::player::emotion::EmotionList> emotions = player.getEmotions();
	if (emotions == nullptr) // Java: player.getEmotions().add on null
		throw runtime::NullPointerException("Player.getEmotions()");
	// Java: (int) (System.currentTimeMillis() / 1000) + minutes * 60, int arithmetic
	emotions->add(emotionId,
		minutes == 0 ? 0
					 : static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000)) +
						   static_cast<uint32_t>(minutes) * 60u),
		true);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem->getL10n()));
	player.getInventory().delete_(*parentItem);
}

} // namespace aion::gameserver::model::templates::item::actions
