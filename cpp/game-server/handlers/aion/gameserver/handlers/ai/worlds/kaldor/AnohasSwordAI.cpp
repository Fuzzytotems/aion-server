#include "aion/gameserver/handlers/ai/worlds/kaldor/AnohasSwordAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai::worlds::kaldor {

AION_AI(AnohasSwordAI, "anohas_sword");

// Java AnohasSwordAI.java:35-42
void AnohasSwordAI::handleDialogStart(Player& player) {
	int32_t fortressLegionId = SiegeService::getInstance().getFortress(7011)->getLegionId();
	if (player.getLegion() != nullptr && player.getLegion()->getLegionId() == fortressLegionId && player.getLegionMember()->isBrigadeGeneral())
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
	else
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java AnohasSwordAI.java:44-73
bool AnohasSwordAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	if (dialogActionId != model::DialogAction::SETPRO1)
		return false;

	if (!player.getInventory().decreaseByItemId(185000215, 1)) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
		return false;
	}

	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 2375));
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LDF5_FORTRESS_NAMED_SPAWN_ITEM());
	World::getInstance().forEachPlayer([](Player& receiver) {
		PacketSendUtility::sendPacket(receiver, SM_SYSTEM_MESSAGE::STR_MSG_LDF5_FORTRESS_NAMED_SPAWN());
		PacketSendUtility::sendPacket(receiver, SM_SYSTEM_MESSAGE::STR_MSG_LDF5_FORTRESS_NAMED_SPAWN_SYSTEM());
	});
	Npc& sword = getOwner();
	NpcController& swordController = sword.getController();
	AIActions::die(*this); // kill sword (starts animation)
	swordController.addTask(TaskId::DECAY, ThreadPoolManager::getInstance().scheduleAtFixedRate({&sword}, [&sword] {
		PacketSendUtility::broadcastPacket(sword, SM_EMOTION(sword, EmotionType::RESURRECT));
		PacketSendUtility::broadcastPacket(sword, SM_EMOTION(sword, EmotionType::DIE));
	}, 10 * 1000, 10 * 1000)); // disable despawn, repeat special die animation instead until anoha spawns
	spawn(702618, 791.38214f, 488.93655f, 143.47617f, int8_t{30}); // map icon
	ThreadPoolManager::getInstance().schedule({this, &sword}, [this, &sword] {
		spawn(855263, 791.38214f, 488.93655f, 143.47517f, int8_t{30}); // Berserk Anoha
		sword.getController().delete_(); // despawn sword
	}, 30, runtime::TimeUnit::MINUTES);
	return true;
}

// Java AnohasSwordAI.java:75-78
bool AnohasSwordAI::ask(AIQuestion question) {
	return question == AIQuestion::ALLOW_RESPAWN ? false : NpcAI::ask(question);
}

} // namespace aion::gameserver::handlers::ai::worlds::kaldor
