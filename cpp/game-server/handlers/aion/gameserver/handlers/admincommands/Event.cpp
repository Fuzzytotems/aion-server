#include "aion/gameserver/handlers/admincommands/Event.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Event);

// Java Event.java:25-37
Event::Event()
	: AdminCommand("event", "Manages event functions and player event states.",
		  "setStatus [name] - Disables AP gain/loss for the given player and sets him to event state.\n"
		  "setGroupStatus [name] - Gets and sets the group of the given player to event state and disables AP gain/loss for them.\n"
		  "setEnemy <cancel|team|ffa> [name] - Sets the specific state (cancel: normal, team: everyone outside the players team is an enemy, ffa: "
		  "everyone is an enemy).\n"
		  "pvpSpawn [asmo|elyos] - Sets a resurrection point for the given race.\n"
		  "clearInstance - Clears the whole instance you have created.\n"
		  "announce <text> - Sends a yellow message for all players in event state.\n"
		  "list - Lists all players in event state.\n"
		  "removeAll - Removes all players from event state.\n") {
}

// Java Event.java:38-123
void Event::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.size() == 0) { // parity= if (params.length == 0) {
		sendInfo(admin);
		return;
	}

	if (equalsIgnoreCase(params[0], "pvpSpawn")) { // parity= if (params[0].equalsIgnoreCase("pvpSpawn")) {
		if (params.size() < 2) // parity: Java's ArrayIndexOutOfBoundsException of params[1], explicit
			throw runtime::IndexOutOfBoundsException("Index 1 out of bounds for length 1"); // parity: (the same)
		if (equalsIgnoreCase(params[1], "asmo")) { // parity= if (params[1].equalsIgnoreCase("asmo")) {
			TeleportService::setEventPos(*admin.getPosition(), Race::ASMODIANS);
			sendInfo(admin, "Eventspawn for Asmodians was set!");
		} else if (equalsIgnoreCase(params[1], "elyos") || equalsIgnoreCase(params[1], "ely")) { // parity= } else if (params[1].equalsIgnoreCase("elyos") || params[1].equalsIgnoreCase("ely")) {
			TeleportService::setEventPos(*admin.getPosition(), Race::ELYOS);
			sendInfo(admin, "Eventspawn for Elyos was set!");
		} else {
			sendInfo(admin, "Invalid race parameter!");
		}
	} else if (equalsIgnoreCase(params[0], "clearInstance")) { // parity= } else if (params[0].equalsIgnoreCase("clearInstance")) {
		clearInstance(admin);
	} else if (equalsIgnoreCase(params[0], "announce")) { // parity= } else if (params[0].equalsIgnoreCase("announce")) {
		std::string sb; // parity= StringBuilder sb = new StringBuilder();
		sb += name(admin) + ':'; // parity= sb.append(name(admin)).append(':');
		for (size_t i = 1; i < params.size(); i++) // parity= for (int i = 1; i < params.length; i++)
			sb += " " + params[i]; // parity= sb.append(" ").append(params[i]);

		World::getInstance().forEachPlayer([&admin, &sb](Player& p) { // parity= World.getInstance().forEachPlayer(p -> {
			if (p.isInCustomState(CustomPlayerState::EVENT_MODE) || &p == &admin) // parity= if (p.isInCustomState(CustomPlayerState.EVENT_MODE) || p == admin)
				PacketSendUtility::sendMessage(p, sb, ChatType::BRIGHT_YELLOW_CENTER); // parity= PacketSendUtility.sendMessage(p, sb.toString(), ChatType.BRIGHT_YELLOW_CENTER);
		}); // parity: (the same)
	} else if (equalsIgnoreCase(params[0], "list")) { // parity= } else if (params[0].equalsIgnoreCase("list")) {
		std::string sb = "Players in event state:"; // parity= StringBuilder sb = new StringBuilder("Players in event state:");
		for (const runtime::Ptr<Player>& p : World::getInstance().getAllPlayers()) // parity= World.getInstance().getAllPlayers().stream().filter(p -> p.isInCustomState(CustomPlayerState.EVENT_MODE)).forEach(p -> sb.append("\n\t").append(name(p)));
			if (p->isInCustomState(CustomPlayerState::EVENT_MODE)) // parity: (the same)
				sb += "\n\t" + name(*p); // parity: (the same)
		sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
	} else if (equalsIgnoreCase(params[0], "removeAll")) { // parity= } else if (params[0].equalsIgnoreCase("removeAll")) {
		for (const runtime::Ptr<Player>& player : World::getInstance().getAllPlayers()) // parity= for (Player player : World.getInstance().getAllPlayers())
			setEventState(admin, *player, true);
	} else if (equalsIgnoreCase(params[0], "setStatus")) { // parity= } else if (params[0].equalsIgnoreCase("setStatus")) {
		runtime::Ptr<Player> player = getPlayer(admin, params.size() > 1 ? std::optional<std::string>(params[1]) : std::nullopt); // parity= Player player = getPlayer(admin, params.length > 1 ? params[1] : null);
		if (player == nullptr)
			return;
		setEventState(admin, *player, false);
	} else if (equalsIgnoreCase(params[0], "setGroupStatus")) { // parity= } else if (params[0].equalsIgnoreCase("setGroupStatus")) {
		runtime::Ptr<Player> player = getPlayer(admin, params.size() > 1 ? std::optional<std::string>(params[1]) : std::nullopt); // parity= Player player = getPlayer(admin, params.length > 1 ? params[1] : null);
		if (player == nullptr)
			return;
		runtime::Ptr<TemporaryPlayerTeam> team = player->getCurrentTeam(); // parity= TemporaryPlayerTeam<?> team = player.getCurrentTeam();
		if (team == nullptr) {
			sendInfo(admin, "The target is not in a group or alliance!");
			return;
		}
		for (const runtime::Ptr<Player>& p : team->getOnlineMembers()) // parity= for (Player p : team.getOnlineMembers())
			setEventState(admin, *p, false);
	} else if (params.size() > 1 && equalsIgnoreCase(params[0], "setEnemy")) { // parity= } else if (params.length > 1 && params[0].equalsIgnoreCase("setEnemy")) {
		runtime::Ptr<Player> player = getPlayer(admin, params.size() > 2 ? std::optional<std::string>(params[2]) : std::nullopt); // parity= Player player = getPlayer(admin, params.length > 2 ? params[2] : null);
		if (player == nullptr)
			return;
		if (!player->isInCustomState(CustomPlayerState::EVENT_MODE)) {
			sendInfo(admin, name(*player) + " is not in event state");
			return;
		}
		bool ffaTeamMode = false;
		std::string msg = "no longer in FFA state.";
		if (equalsIgnoreCase(params[1], "cancel")) { // parity= if (params[1].equalsIgnoreCase("cancel")) {
			player->unsetCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
		} else if (equalsIgnoreCase(params[1], "team")) { // parity= } else if (params[1].equalsIgnoreCase("team")) {
			player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
			msg = "in Team-FFA state now.";
			ffaTeamMode = true;
		} else if (equalsIgnoreCase(params[1], "ffa")) { // parity= } else if (params[1].equalsIgnoreCase("ffa")) {
			player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
			msg = "in FFA state now.";
			model::team::group::PlayerGroupService::removePlayer(*player); // parity= PlayerGroupService.removePlayer(player);
			model::team::alliance::PlayerAllianceService::removePlayer(*player); // parity= PlayerAllianceService.removePlayer(player);
		} else {
			sendInfo(admin);
			return;
		}
		player->setInFfaTeamMode(ffaTeamMode);
		player->getController().onChangedPlayerAttributes();
		sendInfo(admin, name(*player) + " is " + msg);
		PacketSendUtility::sendMessage(*player, "You are " + msg, ChatType::BRIGHT_YELLOW_CENTER);
	} else {
		sendInfo(admin);
	}
}

// Java Event.java:125-139
runtime::Ptr<Player> Event::getPlayer(Player& admin, std::optional<std::string> name) {
	runtime::Ptr<Player> player = nullptr;
	if (name) { // parity= if (name != null) {
		std::string playerName = Util::convertName(*name);
		player = World::getInstance().getPlayer(playerName);
		if (player == nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		}
	} else if (runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget())) { // parity= } else if (admin.getTarget() instanceof Player target) {
		player = target;
	} else {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
	}
	return player;
}

// Java Event.java:141-159
void Event::clearInstance(Player& admin) {
	runtime::Ptr<WorldMapInstance> map = admin.getPosition()->getWorldMapInstance();
	if (!map->getParent()->isInstanceType()) {
		sendInfo(admin, "This map is not an instance!");
		return;
	}
	if (map->getRegisteredCount() != 1 || !map->isRegistered(admin.getObjectId())) {
		sendInfo(admin, "This instance was not created by you, you cannot delete NPCs here. Use //goto to create a new one!");
		return;
	}
	int32_t count = 0; // parity= AtomicInteger count = new AtomicInteger(); // Java: AtomicInteger (only the lambda's capture needed it)
	map->forEachNpc([&count](Npc& npc) { // parity= map.forEachNpc(npc -> {
		npc.getController().delete_(); // parity= npc.getController().delete();
		count++; // parity= count.getAndIncrement();
	}); // parity: (the same)
	map->forEachDoor([](model::gameobjects::StaticDoor& door) { door.setOpen(true); }); // parity= map.forEachDoor(door -> door.setOpen(true));

	sendInfo(admin, "Deleted " + std::to_string(count) + " NPCs."); // parity= sendInfo(admin, "Deleted " + count + " NPCs.");
}

// Java Event.java:161-177
void Event::setEventState(Player& admin, Player& player, bool onlyRemove) {
	if (player.isInCustomState(CustomPlayerState::EVENT_MODE)) {
		player.unsetCustomState(CustomPlayerState::EVENT_MODE);
		player.unsetCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
		player.setInFfaTeamMode(false);
		player.getController().onChangedPlayerAttributes();
		sendInfo(admin, name(player) + " was removed from event state.");
		PacketSendUtility::sendMessage(player, "You were removed from event state!", ChatType::BRIGHT_YELLOW_CENTER);
	} else if (!onlyRemove) {
		player.setCustomState(CustomPlayerState::EVENT_MODE);
		sendInfo(admin, name(player) + " was set in event state.");
		PacketSendUtility::sendMessage(player,
			"You are in event state now. Please notice that you are not allowed to leave the event without removal of this state!",
			ChatType::BRIGHT_YELLOW_CENTER);
	}
}

} // namespace aion::gameserver::handlers::admincommands
