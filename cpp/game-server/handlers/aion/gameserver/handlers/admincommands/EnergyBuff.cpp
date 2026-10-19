#include "aion/gameserver/handlers/admincommands/EnergyBuff.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(EnergyBuff);

// Java EnergyBuff.java:13-15
EnergyBuff::EnergyBuff() : AdminCommand("energy") {
}

// Java EnergyBuff.java:17-51
void EnergyBuff::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params == null || params.length < 1) {
		info(admin, std::nullopt);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= Player targetPlayer = player.getTarget() instanceof Player target ? target : player;
	Player& targetPlayer = target != nullptr ? *target : admin; // parity: (the same)
	if (params[0] == "refresh") { // parity= if (params[0].equals("refresh")) {
		PacketSendUtility::sendPacket(targetPlayer, SM_STATS_INFO(targetPlayer));
		return;
	}

	if (params.size() < 2 || (params[1] == "add" && params.size() < 3)) { // parity= if (params.length < 2 || (params[1].equals("add") && params.length < 3)) {
		info(admin, std::nullopt);
		return;
	}

	if (params[0] == "repose") { // parity= if (params[0].equals("repose")) {
		if (params[1] == "info") // parity= if (params[1].equals("info"))
			PacketSendUtility::sendMessage(admin, "Current EoR: " + std::to_string(targetPlayer.getCommonData()->getCurrentReposeEnergy()) +
				"\n Max EoR: " + std::to_string(targetPlayer.getCommonData()->getMaxReposeEnergy()));
		else if (params[1] == "add") // parity= else if (params[1].equals("add"))
			targetPlayer.getCommonData()->addReposeEnergy(commons::utils::parseLong(params[2]));
		else if (params[1] == "reset") // parity= else if (params[1].equals("reset"))
			targetPlayer.getCommonData()->setCurrentReposeEnergy(0);
	} else if (params[0] == "salvation") { // parity= } else if (params[0].equals("salvation")) {
		if (params[1] == "info") // parity= if (params[1].equals("info"))
			PacketSendUtility::sendMessage(admin, "Current EoS: " + std::to_string(targetPlayer.getCommonData()->getCurrentSalvationPercent()));
		else if (params[1] == "add") // parity= else if (params[1].equals("add"))
			targetPlayer.getCommonData()->addSalvationPoints(commons::utils::parseLong(params[2]));
		else if (params[1] == "reset") // parity= else if (params[1].equals("reset"))
			targetPlayer.getCommonData()->resetSalvationPoints();
	}
}

// Java EnergyBuff.java:53-57
void EnergyBuff::info(Player& player, std::optional<std::string_view> /*message*/) {
	std::string syntax = "//energy repose|salvation|refresh info|reset|add [points]";
	PacketSendUtility::sendMessage(player, syntax);
}

} // namespace aion::gameserver::handlers::admincommands
