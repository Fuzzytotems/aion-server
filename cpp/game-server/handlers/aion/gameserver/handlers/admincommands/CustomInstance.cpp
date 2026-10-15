#include "aion/gameserver/handlers/admincommands/CustomInstance.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(CustomInstance);

// Java CustomInstance.java:15-22
CustomInstance::CustomInstance()
	: AdminCommand("cinstance", "Utility command for the custom instance.",
		  "removecd - Removes the custom instance cooldown of selected player.\n"
		  "getrank - Gets the current custom instance rank of selected player.\n"
		  "setrank [newRank] - Changes the custom instance rank of selected player to given value.\n") {
}

// Java CustomInstance.java:24-48
void CustomInstance::execute(Player& player, std::span<const std::string> params) {
	using custom::instance::CustomInstanceService; // parity: C++ using-declaration
	if (params.size() < 1) { // parity= if (params.length < 1) {
		sendInfo(player);
		return;
	}
	runtime::Ptr<Player> target = runtime::as<Player>(player.getTarget()); // parity= if (!(player.getTarget() instanceof Player target)) {
	if (target == nullptr) { // parity: (the same)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}
	if (commons::utils::StringUtils::equalsIgnoreCase("removecd", params[0])) { // parity= if ("removecd".equalsIgnoreCase(params[0])) {
		if (CustomInstanceService::getInstance().resetEntryCooldown(target->getObjectId())) {
			sendInfo(player, "Removed custom instance cooldown for " + name(*target) + ".");
		} else {
			sendInfo(player, name(*target) + " does not need a reset.");
		}
	} else if (commons::utils::StringUtils::equalsIgnoreCase("getrank", params[0])) { // parity= } else if ("getrank".equalsIgnoreCase(params[0])) {
		int32_t rank = CustomInstanceService::getInstance().loadOrCreateRank(target->getObjectId()).getRank();
		sendInfo(player, name(*target) + "'s current rank is " + custom::instance::getRankDescription(rank) + " (" + std::to_string(rank) + ")."); // parity= sendInfo(player, name(target) + "'s current rank is " + CustomInstanceRankEnum.getRankDescription(rank) + " (" + rank + ").");
	} else if (commons::utils::StringUtils::equalsIgnoreCase("setrank", params[0]) && params.size() > 1) { // parity= } else if ("setrank".equalsIgnoreCase(params[0]) && params.length > 1) {
		int32_t rank = commons::utils::parseInt(params[1]); // parity= int rank = Integer.parseInt(params[1]);
		CustomInstanceService::getInstance().changePlayerRank(target->getObjectId(), rank, 0);
		sendInfo(player, "Changed " + name(*target) + " to " + std::to_string(rank) + " which is equivalent to " + custom::instance::getRankDescription(rank)); // parity= sendInfo(player, "Changed " + name(target) + " to " + rank + " which is equivalent to " + CustomInstanceRankEnum.getRankDescription(rank));
	} else {
		sendInfo(player);
	}
}

} // namespace aion::gameserver::handlers::admincommands
