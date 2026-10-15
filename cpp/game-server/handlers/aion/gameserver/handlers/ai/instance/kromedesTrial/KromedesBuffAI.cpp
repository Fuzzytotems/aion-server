#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesBuffAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

AION_AI(KromedesBuffAI, "krbuff");

// Java KromedesBuffAI.java:23-45
void KromedesBuffAI::handleUseItemFinish(Player& player) {
	switch (getNpcId()) {
		case 730336:
			SkillEngine::getInstance().applyEffectDirectly(19216, player, player);
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_BUFF_01());
			break;
		case 730337:
			SkillEngine::getInstance().applyEffectDirectly(19217, player, player);
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_BUFF_02());
			AIActions::deleteOwner(*this);
			break;
		case 730338:
			SkillEngine::getInstance().applyEffectDirectly(19218, player, player);
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_BUFF_03());
			AIActions::deleteOwner(*this);
			break;
		case 730339:
			SkillEngine::getInstance().applyEffectDirectly(19219, player, player);
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_BUFF_04());
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
