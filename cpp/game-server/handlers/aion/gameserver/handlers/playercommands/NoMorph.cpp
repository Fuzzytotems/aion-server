#include "aion/gameserver/handlers/playercommands/NoMorph.h"

#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Nomorph);

Nomorph::Nomorph() : PlayerCommand("nomorph", "Enables/disables your current transformation appearance.") {
}

// Java NoMorph.java:14-24 (class Nomorph)
void Nomorph::execute(Player& player, std::span<const std::string> /*params*/) {
	if (player.getTransformModel().getEventModelId() == player.getObjectTemplate()->getTemplateId()) {
		player.getTransformModel().setEventModelId(0);
	} else {
		player.getTransformModel().setEventModelId(player.getObjectTemplate()->getTemplateId());
	}
	player.getTransformModel().updateVisually();
	sendInfo(player, "Transformation appearance is now " + (player.getTransformModel().getEventModelId() == player.getObjectTemplate()->getTemplateId() ? ChatUtil::color("inactive", utils::JavaColor::RED) : ChatUtil::color("active", utils::JavaColor::GREEN)) + "."); // parity= sendInfo(player, "Transformation appearance is now " + (player.getTransformModel().getEventModelId() == player.getObjectTemplate().getTemplateId() ? ChatUtil.color("inactive", Color.RED) : ChatUtil.color("active", Color.GREEN)) + ".");
}

} // namespace aion::gameserver::handlers::playercommands
