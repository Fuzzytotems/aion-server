#include "aion/gameserver/skillengine/action/ItemUseAction.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::action {

using dataholders::DataManager;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;

bool ItemUseAction::act(model::Skill& skill) const {
	if (!expendable)
		return canAct(skill);
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector()); player && !player->getInventory().decreaseByItemId(itemid, count)) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_ITEM(DataManager::ITEM_DATA->getItemTemplate(itemid)->getL10n()));
		return false;
	}
	return true;
}

bool ItemUseAction::canAct(model::Skill& skill) const {
	if (Ptr<Player> player = runtime::as<Player>(skill.getEffector()); player && player->getInventory().getItemCountByItemId(itemid) < count) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_ITEM(DataManager::ITEM_DATA->getItemTemplate(itemid)->getL10n()));
		return false;
	}
	return true;
}

} // namespace aion::gameserver::skillengine::action
