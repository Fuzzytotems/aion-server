#include "aion/gameserver/skillengine/action/DpUseAction.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::action {

using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;

bool DpUseAction::act(model::Skill& skill) const {
	Ptr<Player> player = runtime::as<Player>(skill.getEffector());
	if (!player)
		return true;
	int32_t currentDp = player->getCommonData()->getDp(); // read once, setDp has no lower bound
	if (currentDp <= 0 || currentDp < value) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_DP());
		return false;
	}
	player->getCommonData()->setDp(currentDp - value);
	return true;
}

bool DpUseAction::canAct(model::Skill& skill) const {
	Ptr<Player> player = runtime::as<Player>(skill.getEffector());
	if (!player)
		return true;
	int32_t currentDp = player->getCommonData()->getDp();
	if (currentDp > 0 && currentDp >= value)
		return true;
	utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_DP());
	return false;
}

} // namespace aion::gameserver::skillengine::action
