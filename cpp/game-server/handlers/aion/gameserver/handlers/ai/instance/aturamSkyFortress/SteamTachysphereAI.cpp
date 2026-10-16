#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/SteamTachysphereAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(SteamTachysphereAI, "steam_tachysphere");

// Java SteamTachysphereAI.java:30-40
void SteamTachysphereAI::handleUseItemFinish(Player& player) {
	const runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(player.getRace() == Race::ELYOS ? 18302 : 28302);
	if (qs == nullptr) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
	} else if (qs->getStatus() != QuestStatus::COMPLETE) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
	} else {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
	}
}

// Java SteamTachysphereAI.java:42-54
bool SteamTachysphereAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		const runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(player.getRace() == Race::ELYOS ? 18302 : 28302);
		if (qs != nullptr && qs->getStatus() == QuestStatus::COMPLETE) {
			TeleportService::teleportTo(player, 300240000, 175.28925f, 625.1088f, 901.009f, int8_t{33});
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLAY_MOVIE(false, 0, 0, 471, false));
			SkillEngine::getInstance().getSkill(player, 19502, 1, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		}
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
