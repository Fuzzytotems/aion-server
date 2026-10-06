#include "aion/gameserver/handlers/admincommands/AddTitle.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/templates/TitleTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(AddTitle);

AddTitle::AddTitle()
	: AdminCommand("addtitle", "Adds titles to players.",
		  "<title ID> - Adds the title to your target (defaults to your character, if no player is targeted).\n"
		  "<title ID> <player> - Adds the title to the specified player.\n") {
}

// Java AddTitle.java:28-61
void AddTitle::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1 || params.size() > 2) {
		sendInfo(player);
		return;
	}

	const TitleTemplate* titleTemplate = DataManager::TITLE_DATA->getTitleTemplate(commons::utils::parseInt(params[0]));
	if (titleTemplate == nullptr) {
		sendInfo(player, "Invalid title ID.");
		return;
	}

	runtime::Ptr<Player> target;
	if (params.size() == 2) {
		std::string playerName = Util::convertName(params[1]);
		target = World::getInstance().getPlayer(playerName);
		if (target == nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
			return;
		}
	} else {
		runtime::Ptr<Player> playerTarget = runtime::as<Player>(player.getTarget());
		target = playerTarget != nullptr ? playerTarget : runtime::Ptr<Player>(&player);
	}

	if (!target->getTitleList().addTitle(titleTemplate->getTitleId(), false, 0)) {
		if (!target->equals(player))
			sendInfo(player, "Couldn't add title \"" + titleTemplate->getL10n() + "\" to " + name(*target));
	} else {
		if (!target->equals(player)) {
			sendInfo(player, "Added title \"" + titleTemplate->getL10n() + "\" to " + name(*target));
			sendInfo(*target, name(player) + " gave you the title \"" + titleTemplate->getL10n() + "\"");
		}
	}
}

} // namespace aion::gameserver::handlers::admincommands
