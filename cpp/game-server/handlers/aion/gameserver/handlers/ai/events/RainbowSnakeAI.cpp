#include "aion/gameserver/handlers/ai/events/RainbowSnakeAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(RainbowSnakeAI, "rainbow_snake");

// Java RainbowSnakeAI.java:25-30
void RainbowSnakeAI::handleCreatureDetected(Creature& creature) {
	GeneralNpcAI::handleCreatureDetected(creature);
	runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature));
	if (player != nullptr && messagedPlayers.add(player->getObjectId()))
		PacketSendUtility::sendMessage(*player, getOwner(), 1501203); // Hey, you there... Yeah, you... Come here!
}

// Java RainbowSnakeAI.java:32-36
void RainbowSnakeAI::handleCreatureNotSee(Creature& creature) {
	GeneralNpcAI::handleCreatureNotSee(creature);
	messagedPlayers.remove(creature.getObjectId());
}

// Java RainbowSnakeAI.java:38-54
bool RainbowSnakeAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId != model::DialogAction::SETPRO1)
		return false;
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(0, 0));
	switch (getNpcId()) {
		case 832963:
		case 832974: { // Serpente Rumin (FRI-SUN)
			// [Event] Rainbow Snake's Splendor (Drop rate +50%)
			SkillEngine::getInstance().getSkill(getOwner(), 10976, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			// [Event] Rainbow Snake's Grace (Gathering XP +100%)
			SkillEngine::getInstance().getSkill(getOwner(), 10977, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			break;
		}
		case 832964:
		case 832975: { // Ahas Rumin (MON-THU)
			// [Event] Rainbow Snake's Love (Crafting XP +100%)
			SkillEngine::getInstance().getSkill(getOwner(), 10978, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			// [Event] Rainbow Snake's Judgement (AP +50%)
			SkillEngine::getInstance().getSkill(getOwner(), 10979, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			break;
		}
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::events
