#include "aion/gameserver/handlers/playercommands/NoExp.h"

#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(NoExp);

NoExp::NoExp() : PlayerCommand("noexp", "Enables/disables your ability to gain experience.") {
}

// Java NoExp.java:18-23
void NoExp::execute(Player& player, std::span<const std::string> /*params*/) {
	runtime::Ptr<PlayerCommonData> pcd = player.getCommonData();
	pcd->setNoExp(!pcd->getNoExp());
	sendInfo(player, "Experience rewards are now " + (pcd->getNoExp() ? ChatUtil::color("inactive", utils::JavaColor::RED) : ChatUtil::color("active", utils::JavaColor::GREEN)) + "."); // parity= sendInfo(player, "Experience rewards are now " + (pcd.getNoExp() ? ChatUtil.color("inactive", Color.RED) : ChatUtil.color("active", Color.GREEN)) + ".");
}

} // namespace aion::gameserver::handlers::playercommands
