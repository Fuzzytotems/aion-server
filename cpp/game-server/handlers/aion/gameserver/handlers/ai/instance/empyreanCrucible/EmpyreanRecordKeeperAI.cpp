#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/EmpyreanRecordKeeperAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/instance/StageType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(EmpyreanRecordKeeperAI, "empyreanrecordkeeper");

// Java EmpyreanRecordKeeperAI.java:25-63
void EmpyreanRecordKeeperAI::handleSpawned() {
	NpcAI::handleSpawned();
	int32_t msg = 0;
	switch (getNpcId()) {
		case 799568:
			msg = 1111460;
			break;
		case 799569:
			msg = 1111461;
			break;
		case 205331:
			msg = 1111462;
			break;
		case 205338:
			msg = 1111463;
			break;
		case 205339:
			msg = 1111464;
			break;
		case 205340:
			msg = 1111465;
			break;
		case 205341:
			msg = 1111466;
			break;
		case 205342:
			msg = 1111467;
			break;
		case 205343:
			msg = 1111468;
			break;
		case 205344:
			msg = 1111469;
			break;
	}
	if (msg != 0) {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), msg, 1000);
	}
}

// Java EmpyreanRecordKeeperAI.java:65-68
void EmpyreanRecordKeeperAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java EmpyreanRecordKeeperAI.java:70-135 (the instance handler is never null: every WorldMapInstance has one)
bool EmpyreanRecordKeeperAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	runtime::Ptr<gameserver::instance::handlers::InstanceHandler> instanceHandler = getPosition()->getWorldMapInstance()->getInstanceHandler();
	if (dialogActionId == model::DialogAction::SETPRO1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		switch (getNpcId()) {
			case 799567:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_1_ELEVATOR);
				break;
			case 799568:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_2_ELEVATOR);
				break;
			case 799569:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_3_ELEVATOR);
				break;
			case 205331:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_4_ELEVATOR);
				break;
			case 205338: // teleport to stage 5
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_5);
				break;
			case 205332:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_5_ROUND_1);
				break;
			case 205339: // teleport to stage 6
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_6);
				break;
			case 205333:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_6_ROUND_1);
				break;
			case 205340: // teleport to stage 7
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_7);
				break;
			case 205334:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_7_ROUND_1);
				break;
			case 205341: // teleport to stage 8
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_8);
				break;
			case 205335:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_8_ROUND_1);
				break;
			case 205342: // teleport to stage 9
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_9);
				break;
			case 205336:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_9_ROUND_1);
				break;
			case 205343: // teleport to stage 9
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_10);
				break;
			case 205337:
				instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_10_ROUND_1);
				break;
			case 205344: // get score
				getPosition()->getWorldMapInstance()->getInstanceHandler()->doReward(player);
				break;
		}
		AIActions::deleteOwner(*this);
	} else if (dialogActionId == model::DialogAction::SETPRO2 && getNpcId() == 799567) { // start with stage 7
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		instanceHandler->onChangeStage(model::instance::StageType::START_STAGE_7);
		AIActions::deleteOwner(*this);
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
