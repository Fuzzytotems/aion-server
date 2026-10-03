#include "aion/gameserver/model/templates/item/actions/SkillLearnAction.h"

#include <optional>

#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

/** Java SkillLearnAction.validateClass (private, SkillLearnAction.java:66-68): no class, the class itself or its starting class */
bool validateClass(const std::optional<PlayerClass>& playerClass, PlayerClass pc) {
	return !playerClass || *playerClass == pc || *playerClass == getStartingClass(pc);
}

} // namespace

// Java SkillLearnAction.java:31-51
bool SkillLearnAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// 1. check player level
	if (player.getCommonData()->getLevel() < level)
		return false;

	PlayerClass pc = player.getCommonData()->getPlayerClass();

	if (!validateClass(playerClass, pc))
		return false;

	// 4. check player race and Race.PC_ALL
	Race race = parentItem->getItemTemplate()->getRace();
	if (player.getRace() != race && race != Race::PC_ALL)
		return false;

	// 5. check whether this skill is already learned
	if (player.getSkillList()->isSkillPresent(skillid))
		return false;

	return true;
}

// Java SkillLearnAction.java:53-64
void SkillLearnAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	utils::PacketSendUtility::broadcastPacket(player,
		network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), parentItem->getItemId()), true);

	// add skill (the "you learned" message is embedded in SM_SKILL_LIST, sent by SkillLearnService)
	services::SkillLearnService::learnSkillBook(player, skillid);

	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem->getL10n()));

	// remove book from inventory (assuming its not stackable)
	runtime::Ptr<gameobjects::Item> item = player.getInventory().getItemByObjId(parentItem->getObjectId());
	if (!item) // Java: Storage.delete(null) dereferences it
		throw runtime::NullPointerException("the skill book " + std::to_string(parentItem->getObjectId()) + " is no longer in the inventory");
	player.getInventory().delete_(*item);
}

} // namespace aion::gameserver::model::templates::item::actions
