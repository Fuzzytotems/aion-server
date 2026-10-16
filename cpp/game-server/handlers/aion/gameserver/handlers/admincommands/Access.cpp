#include "aion/gameserver/handlers/admincommands/Access.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/services/CommandsAccessService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Access);

// parity= private final Map<Integer, Byte> oldAccessLevels = new HashMap<>(); (Access.h)

// Java Access.java:20-29
Access::Access()
	: AdminCommand("access", "Chat command and access level management.",
		  "add <player name> <command name> - Grants the player access to the given chat command.\n"
		  "remove <player name> <command name> - Removes the player's access to the given chat command.\n"
		  "removeall <player name> - Removes all granted accesses.\n"
		  "level <number> - Temporarily sets your accesslevel to a given lower value (for test purposes).\n"
		  "level reset - Resets your accesslevel to the original value.\n") {
}

// Java Access.java:31-111
void Access::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() <= 1) { // parity= if (params.length <= 1) {
		sendInfo(admin);
		return;
	}
	std::string cmd = commons::utils::StringUtils::toLowerCase(params[0]);
	if (cmd == "add" || cmd == "remove") { // parity= switch (cmd) { case "add": case "remove": {
		if (params.size() < 3) { // parity= if (params.length < 3) {
			sendInfo(admin);
			return;
		}
		std::string playerName = Util::convertName(params[1]);
		const std::string& commandName = params[2]; // parity= String commandName = params[2];
		runtime::Ref<PlayerCommonData> pcd = PlayerService::getOrLoadPlayerCommonData(playerName); // parity= PlayerCommonData pcd = PlayerService.getOrLoadPlayerCommonData(playerName);
		if (pcd == nullptr) {
			sendInfo(admin, "Player with name " + playerName + " doesn't exists!");
			return;
		}
		if (cmd == "add") // parity= if (cmd.equals("add"))
			CommandsAccessService::giveAccess(admin, pcd->getPlayerObjId(), commandName);
		else
			CommandsAccessService::removeAccess(admin, pcd->getPlayerObjId(), commandName);
	} else if (cmd == "removeall") { // parity= break; } case "removeall": {
		std::string playerName = Util::convertName(params[1]);
		runtime::Ref<PlayerCommonData> pcd = PlayerService::getOrLoadPlayerCommonData(playerName); // parity= PlayerCommonData pcd = PlayerService.getOrLoadPlayerCommonData(playerName);
		if (pcd == nullptr) {
			sendInfo(admin, "Player with name " + playerName + " doesn't exists!");
			return;
		}
		if (CommandsAccessService::removeAllAccesses(pcd->getPlayerObjId()))
			sendInfo(admin, "Removed all custom chat command accesses from player " + pcd->getName());
		else
			sendInfo(admin, "Nothing to remove from player " + pcd->getName());
	} else if (cmd == "level") { // parity= break; } case "level": {
		int8_t currentLevel = admin.getAccount()->getAccessLevel();
		int8_t maxLevel = oldAccessLevels.getOrDefault(admin.getObjectId(), currentLevel);
		if (commons::utils::StringUtils::equalsIgnoreCase("reset", params[1])) { // parity= if ("reset".equalsIgnoreCase(params[1])) {
			if (maxLevel == currentLevel) {
				sendInfo(admin, "Nothing to reset.");
				return;
			}
			admin.getAccount()->setAccessLevel(maxLevel);
			sendInfo(admin, "Your access level has been reset.");
		} else {
			int32_t parsed = commons::utils::parseInt(params[1]); // parity= byte level = Byte.parseByte(params[1]);
			if (parsed < -128 || parsed > 127) // parity: Byte.parseByte's range check
				throw commons::utils::NumberFormatException("Value out of range. Value:\"" + params[1] + "\" Radix:10"); // parity: (the same)
			int8_t level = static_cast<int8_t>(parsed); // parity: (the same)
			if (level < 0 || level > maxLevel) {
				sendInfo(admin, "Invalid access level.");
				return;
			}
			if (level == currentLevel) {
				sendInfo(admin, "You already have access level " + std::to_string(level) + "."); // parity= sendInfo(admin, "You already have access level " + level + ".");
				return;
			}
			admin.getAccount()->setAccessLevel(level);
			if (level < getLevel() && !CommandsAccessService::hasAccess(admin.getObjectId(), getAlias())) {
				CommandsAccessService::giveTemporaryAccess(admin, admin.getObjectId(), getAlias());
			}
			sendInfo(admin, "Your access level has been changed.");
		}
		currentLevel = admin.getAccount()->getAccessLevel();
		if (currentLevel == maxLevel) {
			oldAccessLevels.remove(admin.getObjectId());
		} else {
			oldAccessLevels.putIfAbsent(admin.getObjectId(), maxLevel);
		}
		if (currentLevel > getLevel() && CommandsAccessService::hasAccess(admin.getObjectId(), getAlias())) {
			CommandsAccessService::removeAccess(admin, admin.getObjectId(), getAlias());
		}
		admin.getController().onChangedPlayerAttributes();
	} else { // parity= break; } default:
		sendInfo(admin);
	}
}

} // namespace aion::gameserver::handlers::admincommands
