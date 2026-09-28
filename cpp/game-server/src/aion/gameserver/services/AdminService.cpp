#include "aion/gameserver/services/AdminService.h"

#include <algorithm>
#include <fstream>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

static const auto itemLog = commons::logging::LoggerFactory::getLogger("GMITEMRESTRICTION");

namespace {

/** Java String.trim() */
std::string_view javaTrim(std::string_view text) {
	while (!text.empty() && static_cast<unsigned char>(text.front()) <= ' ')
		text.remove_prefix(1);
	while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ')
		text.remove_suffix(1);
	return text;
}

} // namespace

AdminService::AdminService() {
	reload();
}

AdminService::~AdminService() = default;

AdminService& AdminService::getInstance() {
	static AdminService instance; // Java SingletonHolder
	return instance;
}

void AdminService::reload() {
	list.clear();

	// Java: try (BufferedReader br = new BufferedReader(new FileReader(...))) { ... } catch (IOException e) { e.printStackTrace(); }
	std::ifstream br("./config/administration/item.restriction.txt", std::ios::binary);
	if (!br) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.AdminService")
			.error("java.io.FileNotFoundException: ./config/administration/item.restriction.txt (cannot open the file)");
	} else {
		std::string line;
		while (std::getline(br, line)) {
			if (!line.empty() && line.back() == '\r') // BufferedReader.readLine strips \r\n
				line.pop_back();
			if (line.starts_with("#") || javaTrim(line).empty())
				continue;

			std::string pt = line.substr(0, line.find('#')); // Java: line.split("#")[0]
			std::erase(pt, ' ');                               // Java: replaceAll(" ", "")
			list.add(commons::utils::parseInt(pt));
		}
	}
	commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.AdminService")
		.info("AdminService loaded " + std::to_string(list.size()) + " operational items.");
}

bool AdminService::canOperate(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target,
	model::gameobjects::Item& item, std::string_view type) {
	return canOperate(player, target, item.getItemId(), type);
}

bool AdminService::canOperate(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target, int32_t itemId,
	std::string_view type) {
	if (!player.isStaff())
		return true;

	if (player.hasAccess(configs::administration::AdminConfig::UNRESTRICTED_ITEMTRADE.load())) // staff member is allowed to trade with who ever he wants to
		return true;

	if (target && target->isStaff()) // allow between server staff
		return true;

	if (list.contains(itemId)) { // item goes from staff member to normal player, so log it
		itemLog.info(player.toString() + " traded item " + std::to_string(itemId) + " via " + std::string(type) +
			(target ? " to player " + target->toString() : ""));
		return true;
	}

	utils::PacketSendUtility::sendMessage(player, "You cannot use " + std::string(type) + " with this item.");
	return false;
}

} // namespace aion::gameserver::services
