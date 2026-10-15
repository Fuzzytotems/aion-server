#include "aion/gameserver/network/aion/clientpackets/CM_APPEARANCE.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/dao/OldNamesDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.h"
#include "aion/gameserver/model/templates/item/actions/CosmeticItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RENAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_APPEARANCE::CM_APPEARANCE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_APPEARANCE.java:37-50
void CM_APPEARANCE::readImpl() {
	type = readC();
	readC();
	readH();
	itemObjId = readD();
	switch (type) {
		case 0:
		case 1:
			newName = readS();
			break;
	}
}

// Java CM_APPEARANCE.java:52-67
void CM_APPEARANCE::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();

	switch (type) {
		case 0: // Change Char Name
			tryChangeCharacterName(*player, utils::Util::convertName(newName), itemObjId);
			break;
		case 1: // Change Legion Name
			tryChangeLegionName(*player, newName, itemObjId);
			break;
		case 2: // cosmetic items
			tryUseCosmeticItem(*player, itemObjId);
			break;
	}
}

// Java CM_APPEARANCE.java:69-88
void CM_APPEARANCE::tryChangeCharacterName(model::gameobjects::player::Player& player, const std::string& name, int32_t itemObjIdValue) {
	using serverpackets::SM_SYSTEM_MESSAGE;
	const std::string oldName = player.getName();
	if (oldName == name)
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_SAME_YOUR_NAME());
	else if (!services::NameRestrictionService::isValidName(name) || services::NameRestrictionService::isForbidden(name))
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_WRONG_INPUT());
	else if (services::player::PlayerService::isNameUsedOrReserved(std::string_view(oldName), name))
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ALREADY_EXIST());
	else {
		runtime::Ptr<model::gameobjects::Item> ticket = player.getInventory().getItemByObjId(itemObjIdValue);
		if (ticket == nullptr) // Java: getItemByObjId(itemObjId).getItemId() on null
			throw runtime::NullPointerException("Inventory.getItemByObjId(" + std::to_string(itemObjIdValue) + ")");
		if ((ticket->getItemId() != 169670000 && ticket->getItemId() != 169670001) || !player.getInventory().decreaseByObjectId(itemObjIdValue, 1))
			utils::audit::AuditLogger::log(player, "tried to rename himself without coupon");
		else {
			dao::OldNamesDAO::insertNames(player.getObjectId(), oldName, name);

			player.getCommonData()->setName(name);
			dao::PlayerDAO::storePlayer(player);
			onPlayerNameChanged(player, oldName);
		}
	}
}

// Java CM_APPEARANCE.java:90-98
void CM_APPEARANCE::onPlayerNameChanged(model::gameobjects::player::Player& player, std::string_view oldName) {
	world::World::getInstance().updateCachedPlayerName(oldName, player);
	if (player.isLegionMember()) {
		services::LegionService::getInstance().addHistory(*player.getLegion(), oldName, model::team::legion::LegionHistoryAction::CHARACTER_RENAME,
			player.getName());
		player.getLegionMember()->setPlayerData(player); // no need to broadcast SM_LEGION_UPDATE_MEMBER here, since SM_RENAME already handles it
	}
	utils::PacketSendUtility::broadcastToWorld(serverpackets::SM_RENAME(player, oldName)); // broadcast to world to update all friendlists, housing npcs, etc.
}

// Java CM_APPEARANCE.java:100-107
void CM_APPEARANCE::tryChangeLegionName(model::gameobjects::player::Player& player, const std::string& name, int32_t itemObjIdValue) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	if (legion == nullptr || !player.getLegionMember()->isBrigadeGeneral()) {
		utils::PacketSendUtility::sendPacket(player, serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_EDIT_GUILD_NAME_ERROR_ONLY_MASTER_CAN_CHANGE_NAME());
		return;
	}
	services::LegionService::getInstance().tryRename(*legion, name, player, itemObjIdValue);
}

// Java CM_APPEARANCE.java:109-119
void CM_APPEARANCE::tryUseCosmeticItem(model::gameobjects::player::Player& player, int32_t itemObjIdValue) {
	runtime::Ptr<model::gameobjects::Item> item = player.getInventory().getItemByObjId(itemObjIdValue);
	if (item != nullptr) {
		const model::templates::item::actions::ItemActions* actions = item->getItemTemplate()->getActions();
		if (actions == nullptr) // Java: getActions().getItemActions() on null
			throw runtime::NullPointerException("ItemTemplate.getActions()");
		for (const std::unique_ptr<model::templates::item::actions::AbstractItemAction>& action : actions->getItemActions()) {
			if (dynamic_cast<const model::templates::item::actions::CosmeticItemAction*>(action.get()) != nullptr && action->canAct(player, nullptr, nullptr)) {
				action->act(player, nullptr, item);
				break;
			}
		}
	}
}

AION_CLIENT_PACKET(CM_APPEARANCE);

} // namespace aion::gameserver::network::aion::clientpackets
