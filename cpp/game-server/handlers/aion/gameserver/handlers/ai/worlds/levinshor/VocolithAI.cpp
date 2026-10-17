#include "aion/gameserver/handlers/ai/worlds/levinshor/VocolithAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/services/RespawnService.h"

namespace aion::gameserver::handlers::ai::worlds::levinshor {

AION_AI(VocolithAI, "LDF4_Advance_Vocolith");

/**
 * Java: the anonymous ItemUseObserver of handleDialogStart (VocolithAI.java:41-50, fieldmap ai.worlds.levinshor.VocolithAI$1), stored in the
 * player's ObserveController until the use task or abort() removes it (a logout's ObserveController::clearWithoutNotify drops it). It captures
 * the AI (Java's enclosing instance, whose Ref retains the npc) and the player.
 */
struct VocolithAI_ItemUseObserver final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<VocolithAI> vocolithAI; // captured this
	const runtime::Ref<Player> player;         // captured param Player player

	static runtime::Ref<VocolithAI_ItemUseObserver> create(VocolithAI& vocolithAI, Player& player) {
		return runtime::makeRef<VocolithAI_ItemUseObserver>(vocolithAI, player);
	}

	// Java VocolithAI.java:43-49
	void abort() override {
		player->getController().cancelTask(TaskId::ACTION_ITEM_NPC);
		PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::END_QUESTLOOT, 0, vocolithAI->getObjectId()), true);
		PacketSendUtility::sendPacket(*player, SM_USE_OBJECT(player->getObjectId(), vocolithAI->getObjectId(), 0, 2));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	VocolithAI_ItemUseObserver(VocolithAI& vocolithAIValue, Player& playerValue)
		: vocolithAI(runtime::Ref<VocolithAI>(vocolithAIValue)), player(runtime::Ref<Player>(playerValue)) {}
	~VocolithAI_ItemUseObserver() override = default;
};

// Java VocolithAI.java:38-62
void VocolithAI::handleDialogStart(Player& player) {
	if (!used.get()) {
		const runtime::Ref<VocolithAI_ItemUseObserver> observer = VocolithAI_ItemUseObserver::create(*this, player);
		int32_t delay = 1500;
		player.getObserveController()->attach(*observer);
		PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), delay, 1));
		PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);
		// Java lambda VocolithAI.java:55-60: pins this, the player and the observer, and captures delay
		VocolithAI_ItemUseObserver& itemUseObserver = *observer;
		player.getController().addTask(TaskId::ACTION_ITEM_NPC,
			ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver}, [this, &player, &itemUseObserver, delay] {
				PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::END_QUESTLOOT, 0, getObjectId()), true);
				PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), delay, 2));
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
				player.getObserveController()->removeObserver(itemUseObserver);
			}, delay));
	}
}

// Java VocolithAI.java:64-77
bool VocolithAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		if (player.getInventory().getItemCountByItemId(185000216) == 0) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_FNAMED_FAIL());
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
			return true;
		} else if (used.compareAndSet(false, true)) {
			handleUsed(player);
		}
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java VocolithAI.java:79-87
void VocolithAI::handleUsed(Player& player) {
	if (player.getInventory().decreaseByItemId(185000216, 1)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_FNAMED_SPAWN_ITEM());
		spawnRandomBoss();
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_FNAMED_FAIL());
		used.set(false);
	}
}

// Java VocolithAI.java:89-98
void VocolithAI::spawnRandomBoss() {
	Npc& vocolith = getOwner();
	AIActions::die(*this); // kill vocolith
	RespawnService::scheduleDecayTask(vocolith, 7500); // schedule late despawn to show full death animation
	ThreadPoolManager::getInstance().schedule({this, &vocolith}, [this, &vocolith] { // schedule boss spawn
		int32_t npcId = 235217 + commons::utils::Rnd::get(0, 3);
		runtime::Ptr<Npc> boss = runtime::cast<Npc>(spawn(npcId, vocolith.getX(), vocolith.getY(), vocolith.getZ(), vocolith.getHeading()));
		PacketSendUtility::broadcastToMap(*boss, SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_FNAMED_SPAWN());
	}, 5000);
}

} // namespace aion::gameserver::handlers::ai::worlds::levinshor
