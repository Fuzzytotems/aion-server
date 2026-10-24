#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/KharunAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(KharunAI, "kharun");

// Java KharunAI.java:25-28
void KharunAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java KharunAI.java:30-38
bool KharunAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		AIActions::deleteOwner(*this);
		startKharunEvent();
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java KharunAI.java:40-56 (the anonymous Runnable: a task pinning this; the field and the door are not checked for null, as in Java)
void KharunAI::startKharunEvent() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<Npc> aethericField = getPosition()->getWorldMapInstance()->getNpc(730613);
		runtime::Ptr<Npc> strongholdDoor = getPosition()->getWorldMapInstance()->getNpc(730612);
		runtime::Ptr<Npc> Kharun = runtime::cast<Npc>(spawn(800335, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{60}));
		Kharun->setTarget(aethericField);
		SkillEngine::getInstance().getSkill(*Kharun, 20943, 60, aethericField)->useNoAnimationSkill();
		PacketSendUtility::broadcastMessage(Kharun, 1500597, 1000);
		PacketSendUtility::broadcastMessage(Kharun, 1500598, 5000);
		strongholdDoor->getController().die();
		aethericField->getController().delete_();
	}, 3000);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
