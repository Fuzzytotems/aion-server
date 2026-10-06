#include "aion/gameserver/handlers/playercommands/Id.h"

#include <typeinfo>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestCategory.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Id);

Id::Id()
	: PlayerCommand("id", "Shows item/quest/NPC IDs.",
		  " - Shows the ID of the selected object.\n"
		  "<item|quest> - Shows the ID of the specified item or quest.\n") {
}

// Java Id.java:29-75. ITEM_ICON is '' (bag), written as UTF-8
void Id::execute(Player& player, std::span<const std::string> params) {
	const std::string ITEM_ICON = commons::utils::StringUtils::toUtf8(std::u16string(1, u''));
	runtime::Ptr<VisibleObject> target = player.getTarget();
	if (params.empty()) {
		if (target == nullptr) {
			sendInfo(player);
			return;
		}
		if (!player.isStaff() && runtime::as<Npc>(target) == nullptr && runtime::as<Gatherable>(target) == nullptr) { // regular players only see IDs of npcs and gatherables
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
			return;
		}
		std::string msg = utils::simpleClassName(typeid(*target)) + ": " + ChatUtil::path(*target, true);
		if (player.isStaff()) {
			int32_t staticId = target->getSpawn() == nullptr ? 0 : target->getSpawn()->getStaticId();
			msg += " (Object ID: " + std::to_string(target->getObjectId()) + (staticId == 0 ? std::string() : ", Static ID: " + std::to_string(staticId)) + ")";
		}
		sendInfo(player, msg);
		return;
	} else {
		int32_t id = ChatUtil::getItemId(params[0]);
		if (id != 0) {
			const ItemTemplate* template_ = DataManager::ITEM_DATA->getItemTemplate(id);
			if (template_ == nullptr) {
				sendInfo(player, "Invalid item.");
				return;
			}
			sendInfo(player, "Item:" + ITEM_ICON + template_->getL10n() + "\nID: " + std::to_string(id));
			return;
		}
		id = ChatUtil::getQuestId(params[0]);
		if (id != 0) {
			const QuestTemplate* template_ = DataManager::QUEST_DATA->getQuestById(id);
			if (template_ == nullptr) {
				sendInfo(player, "Invalid quest.");
				return;
			}
			sendInfo(player, "Quest:" + getQuestIcon(*template_) + template_->getL10n() + "\nID: " + std::to_string(id));
			return;
		}
	}
	sendInfo(player);
}

// Java Id.java:77-84
std::string Id::getQuestIcon(const QuestTemplate& template_) {
	using model::templates::quest::QuestCategory;
	char16_t icon;
	switch (template_.getCategory()) {
		case QuestCategory::EVENT:
			icon = u''; // pink
			break;
		case QuestCategory::MISSION:
			icon = u''; // golden
			break;
		case QuestCategory::IMPORTANT:
		case QuestCategory::SIGNIFICANT:
			icon = u''; // dark blue
			break;
		default:
			icon = u''; // light blue
	}
	return commons::utils::StringUtils::toUtf8(std::u16string(1, icon));
}

} // namespace aion::gameserver::handlers::playercommands
