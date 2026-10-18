#include "aion/gameserver/handlers/admincommands/RelinquishCraft.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(RelinquishCraft);

// Java RelinquishCraft.java:17-22
RelinquishCraft::RelinquishCraft()
	: AdminCommand("relinquishcraft", "Removes a players crafting expert or master status.",
		  "<skill ID> <expert|master> - Removes your target's master or expert status for the given crafting skill. "
		  "Affects your own character if no player is targeted.\n"
		  "<name> <skill ID> <expert|master> - Removes the player's master or expert status for the given crafting skill.\n") {
}

// Java RelinquishCraft.java:24-62
void RelinquishCraft::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) { // parity= if (params.length < 2) {
		sendInfo(admin);
		return;
	}

	size_t i = 0;
	runtime::Ptr<Player> target;
	if (params.size() == 3) { // parity= if (params.length == 3) {
		std::string playerName = Util::convertName(params[i++]);
		target = World::getInstance().getPlayer(playerName);
		if (target == nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
			return;
		}
	} else {
		runtime::Ptr<Player> player = runtime::as<Player>(admin.getTarget()); // parity= target = admin.getTarget() instanceof Player player ? player : admin;
		target = player != nullptr ? player : runtime::Ptr<Player>(admin); // parity: (the same)
	}

	std::optional<Profession> profession = model::craft::getBySkillId(commons::utils::parseInt(params[i++]));
	if (!profession.has_value() || !model::craft::isCrafting(*profession)) { // parity= if (profession == null || !profession.isCrafting()) {
		sendInfo(admin, "Invalid skill ID.");
		return;
	}

	const std::string professionName(xml::enumName(*profession)); // parity: C++ the enum's toString() for the concatenations below
	if (commons::utils::StringUtils::equalsIgnoreCase("expert", params[i])) { // parity= if ("expert".equalsIgnoreCase(params[i])) {
		if (services::craft::RelinquishCraftStatus::relinquishExpertStatus(*target, *profession, 0))
			sendInfo(admin, "Successfully removed expert status for " + professionName);
		else
			sendInfo(admin, name(*target) + " doesn't have " + professionName + " on expert.");
	} else if (commons::utils::StringUtils::equalsIgnoreCase("master", params[i])) { // parity= } else if ("master".equalsIgnoreCase(params[i])) {
		if (services::craft::RelinquishCraftStatus::relinquishMasterStatus(*target, *profession, 0))
			sendInfo(admin, "Successfully removed master status for " + professionName);
		else
			sendInfo(admin, name(*target) + " doesn't have " + professionName + " on master.");
	} else
		sendInfo(admin);
}

} // namespace aion::gameserver::handlers::admincommands
