#include "aion/gameserver/handlers/consolecommands/Set_vitalpoint.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Set_vitalpoint);

// Java Set_vitalpoint.java:13-15
Set_vitalpoint::Set_vitalpoint() : ConsoleCommand("set_vitalpoint") {
}

// Java Set_vitalpoint.java:17-37
void Set_vitalpoint::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	int32_t value;

	try {
		value = commons::utils::parseInt(params[0]); // parity= value = Integer.parseInt(params[0]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	player.getCommonData()->setCurrentSalvationPoints(value);
	PacketSendUtility::sendPacket(player, SM_STATS_INFO(player)); // parity= PacketSendUtility.sendPacket(player, new SM_STATS_INFO(player));
}

// Java Set_vitalpoint.java:39-42
void Set_vitalpoint::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///set_vitalpoint <value>");
}

} // namespace aion::gameserver::handlers::consolecommands
