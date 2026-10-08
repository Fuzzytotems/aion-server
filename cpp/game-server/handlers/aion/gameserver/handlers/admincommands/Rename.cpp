#include "aion/gameserver/handlers/admincommands/Rename.h"

#include <optional>
#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/dao/OldNamesDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/clientpackets/CM_APPEARANCE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Rename);

Rename::Rename()
	: AdminCommand("rename", "Changes a player's name.",
		  "<new name> - Renames your target.\n"
		  "<player name> <new name> [f] - Renames the given player (f = force rename, ignoring reserved names).\n") {
}

// Java Rename.java:32-64
void Rename::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params.length < 1) {
		sendInfo(admin);
		return;
	}
	std::optional<std::string> oldName = params.size() == 1 ? std::nullopt : std::optional<std::string>(Util::convertName(params[0])); // parity= String oldName = params.length == 1 ? null : Util.convertName(params[0]);
	std::string newName = Util::convertName(params.size() == 1 ? params[0] : params[1]); // parity= String newName = Util.convertName(params.length == 1 ? params[0] : params[1]);
	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget()); // parity: the pattern variable of the instanceof below
	if (!(params.size() == 1 && targetPlayer != nullptr) && oldName == std::nullopt) // parity: World.getPlayer(null) is ConcurrentHashMap.get(null): Java's NullPointerException, explicit
		throw runtime::NullPointerException("ConcurrentHashMap.get(null): World.getPlayer of the null oldName"); // parity: (the same; ChatCommand.run logs it)
	runtime::Ptr<Player> renamed = params.size() == 1 && targetPlayer != nullptr ? targetPlayer : World::getInstance().getPlayer(*oldName); // parity= Player renamed = params.length == 1 && admin.getTarget() instanceof Player player ? player : World.getInstance().getPlayer(oldName);
	runtime::Ptr<PlayerCommonData> renamedCommonData = renamed == nullptr ? runtime::Ptr<PlayerCommonData>(PlayerService::getOrLoadPlayerCommonData(*oldName)) : renamed->getCommonData(); // parity= PlayerCommonData renamedCommonData = renamed == null ? PlayerService.getOrLoadPlayerCommonData(oldName) : renamed.getCommonData();
	if (renamedCommonData == nullptr) {
		PacketSendUtility::sendPacket(admin, oldName == std::nullopt ? SM_SYSTEM_MESSAGE::STR_INVALID_TARGET() : SM_SYSTEM_MESSAGE::STR_NO_USER_NAMED(*oldName)); // parity= PacketSendUtility.sendPacket(admin, oldName == null ? SM_SYSTEM_MESSAGE.STR_INVALID_TARGET() : SM_SYSTEM_MESSAGE.STR_NO_USER_NAMED(oldName));
		return;
	} else {
		oldName = renamedCommonData->getName();
	}
	if (!NameRestrictionService::isValidName(newName) || NameRestrictionService::isForbidden(newName)) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_WRONG_INPUT());
		return;
	}
	int32_t nameReservationDurationDays = params.size() >= 3 && commons::utils::StringUtils::equalsIgnoreCase(params[2], "f") ? 0 : NameConfig::RESERVE_OLD_NAME_DAYS.load(); // parity= int nameReservationDurationDays = params.length >= 3 && params[2].equalsIgnoreCase("f") ? 0 : NameConfig.RESERVE_OLD_NAME_DAYS;
	if (PlayerService::isNameUsedOrReserved(*oldName, newName, nameReservationDurationDays)) { // parity= if (PlayerService.isNameUsedOrReserved(oldName, newName, nameReservationDurationDays)) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ALREADY_EXIST());
		return;
	}
	OldNamesDAO::insertNames(renamedCommonData->getPlayerObjId(), *oldName, newName); // parity= OldNamesDAO.insertNames(renamedCommonData.getPlayerObjId(), oldName, newName);
	renamedCommonData->setName(newName);
	PlayerDAO::storePlayerName(*renamedCommonData);
	if (renamed != nullptr)
		CM_APPEARANCE::onPlayerNameChanged(*renamed, *oldName); // parity= CM_APPEARANCE.onPlayerNameChanged(renamed, oldName);
	sendInfo(admin, *oldName + " has been renamed to " + newName); // parity= sendInfo(admin, oldName + " has been renamed to " + newName);
}

} // namespace aion::gameserver::handlers::admincommands
