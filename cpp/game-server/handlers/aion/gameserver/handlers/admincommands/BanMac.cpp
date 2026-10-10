#include "aion/gameserver/handlers/admincommands/BanMac.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/network/BannedMacManager.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(BanMac);

BanMac::BanMac() : AdminCommand("banmac") {
}

// Java BanMac.java:23-68. `time * 60 * 1000` is Java int arithmetic: the pseudo infinity of 10 years wraps (proposed correction J-CP5-1)
void BanMac::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		info(player, "Please add one or more parameters");
		return;
	}

	int32_t time;
	std::string address;
	std::string targetName = "direct_type";

	// try parsing
	try {
		time = commons::utils::parseInt(params[0]);
		if (time == 0) // 0 is 10 years since system don't allow infinte banns without rework - it's pseudo infinity
			time = 60 * 24 * 365 * 10;
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		info(player, "Please enter a valid integer amount of minutes");
		return;
	}

	// is mac defined?
	if (params.size() > 1) {
		address = params[1];
	} else { // no address defined
		runtime::Ptr<VisibleObject> target = player.getTarget();
		if (runtime::Ptr<Player> targetpl = runtime::as<Player>(target)) { // parity= if (target instanceof Player) {
			if (target->equals(player)) {
				info(player, "Omg, disselect yourself please.");
				return;
			}
			std::shared_ptr<AionConnection> connection = targetpl->getClientConnection();
			if (connection == nullptr) // parity: Java's NullPointerException of targetpl.getClientConnection().getMacAddress(), explicit
				throw runtime::NullPointerException("Player.getClientConnection()"); // parity: (the same)
			address = connection->getMacAddress();
			targetName = targetpl->getName();
			connection->close(); // parity= targetpl.getClientConnection().close();
		} else {
			info(player, "You should select a player or give me any mac address");
			return;
		}
	}

	const int32_t millis = static_cast<int32_t>(static_cast<uint32_t>(time) * 60u * 1000u); // parity: Java's int time * 60 * 1000 (wraps)
	BannedMacManager::getInstance().banAddress(address, commons::utils::currentTimeMillis() + millis, // parity= BannedMacManager.getInstance().banAddress(address, System.currentTimeMillis() + time * 60 * 1000,
		"author=" + player.getName() + ", " + std::to_string(player.getObjectId()) + "; target=" + targetName);
}

// Java BanMac.java:70-76
void BanMac::info(Player& player, std::optional<std::string_view> message) {
	if (!message) // parity: Java's NullPointerException of message.equals(""), explicit
		throw runtime::NullPointerException("message"); // parity: (the same)
	if (*message != "") // parity= if (!message.equals(""))
		PacketSendUtility::sendMessage(player, *message);
	PacketSendUtility::sendMessage(player, "Syntax: //banmac [time in minutes] <mac>");
	PacketSendUtility::sendMessage(player, "Note: 0 minutes will cause permanent ban");
}

} // namespace aion::gameserver::handlers::admincommands
