#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombRoyaltyAdmirerAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombRoyaltyAdmirerAI, "shugo_tomb_royalty_admirer");

// Java ShugoTombRoyaltyAdmirerAI.java:29-32
void ShugoTombRoyaltyAdmirerAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java ShugoTombRoyaltyAdmirerAI.java:34-75 (Java's SETPRO2 case has no break: the switch ends there)
bool ShugoTombRoyaltyAdmirerAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	int32_t instanceId = player.getInstanceId();

	switch (dialogActionId) {
		case model::DialogAction::SETPRO1:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
			switch (getNpcId()) {
				case 831110:
					changeStageList(player, StageList::START_STAGE_1_PHASE_1);
					break;
				case 831111:
					changeStageList(player, StageList::START_STAGE_2_PHASE_1);
					break;
				case 831112:
					changeStageList(player, StageList::START_STAGE_3_PHASE_1);
					break;
				case 831114: // Crown Prince's Delighted Admirer
				case 831306: // Crown Prince's Disappointed Admirer
					TeleportService::teleportTo(player, 300560000, instanceId, 346.27332f, 424.07101f, 294.75793f, int8_t{90}, TeleportAnimation::FADE_OUT_BEAM);
					break;
				case 831115: // Empress' Delighted Admirer
				case 831195: // Empress' Disappointed Admirer
					TeleportService::teleportTo(player, 300560000, instanceId, 450.8527f, 105.94637f, 212.20023f, int8_t{90}, TeleportAnimation::FADE_OUT_BEAM);
					break;
				default:
					break;
			}
			break;
		case model::DialogAction::SETPRO2:
			switch (getNpcId()) {
				case 831114: // Crown Prince's Delighted Admirer
				case 831306: // Crown Prince's Disappointed Admirer
					SkillEngine::getInstance().applyEffectDirectly(player.getRace() == Race::ASMODIANS ? 21104 : 21095, player, player);
					break;
				case 831115: // Empress' Delighted Admirer
				case 831195: // Empress' Disappointed Admirer
					SkillEngine::getInstance().applyEffectDirectly(player.getRace() == Race::ASMODIANS ? 21105 : 21096, player, player);
					break;
				default:
					break;
			}
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
			break;
		default:
			break;
	}
	return true;
}

// Java ShugoTombRoyaltyAdmirerAI.java:77-81
void ShugoTombRoyaltyAdmirerAI::changeStageList(Player& player, StageList stageList) {
	getPosition()->getWorldMapInstance()->getInstanceHandler()->onChangeStageList(stageList);
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
