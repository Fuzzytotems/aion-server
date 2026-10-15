#include "aion/gameserver/handlers/consolecommands/Set_makeup_bonus.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Set_makeup_bonus);

// Java Set_makeup_bonus.java:13-15
Set_makeup_bonus::Set_makeup_bonus() : ConsoleCommand("set_makeup_bonus") {
}

// Java Set_makeup_bonus.java:17-38
void Set_makeup_bonus::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	int64_t value;

	try {
		value = commons::utils::parseLong(params[0]); // parity= value = Long.parseLong(params[0]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	player.getCommonData()->setCurrentReposeEnergy(value);
	PacketSendUtility::sendPacket(player, SM_STATS_INFO(player)); // parity= PacketSendUtility.sendPacket(player, new SM_STATS_INFO(player));
	PacketSendUtility::sendMessage(admin, "Repose Energy set to: " + std::to_string(value)); // parity= PacketSendUtility.sendMessage(admin, "Repose Energy set to: " + value);
}

// Java Set_makeup_bonus.java:40-43
void Set_makeup_bonus::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///set_makeup_bonus <value>");
}

} // namespace aion::gameserver::handlers::consolecommands
