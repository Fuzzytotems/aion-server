#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorAI.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_USE_OBJECT;

/**
 * Java: the anonymous ItemUseObserver of handleCharging (ShieldGeneratorAI.java, fieldmap ShieldGeneratorAI$1): it aborts the charge on any
 * action of the player. It captures the AI (Java's enclosing instance) and the player.
 */
struct ShieldGeneratorAI_ItemUseObserver final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ShieldGeneratorAI> shieldGeneratorAI; // captured this
	const runtime::Ref<Player> player;                       // captured param Player player

	static runtime::Ref<ShieldGeneratorAI_ItemUseObserver> create(ShieldGeneratorAI& shieldGeneratorAI, Player& player) {
		return runtime::makeRef<ShieldGeneratorAI_ItemUseObserver>(shieldGeneratorAI, player);
	}

	// Java ShieldGeneratorAI.java:103-111
	void abort() override {
		player->getController().cancelTask(TaskId::ACTION_ITEM_NPC);
		PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::END_QUESTLOOT, 0, shieldGeneratorAI->getObjectId()), true);
		PacketSendUtility::sendPacket(*player, SM_USE_OBJECT(player->getObjectId(), shieldGeneratorAI->getObjectId(), 0, 2));
		player->getObserveController()->removeObserver(*this);
		shieldGeneratorAI->isUnderCharge.set(false);
	}

protected:
	ShieldGeneratorAI_ItemUseObserver(ShieldGeneratorAI& shieldGeneratorAIValue, Player& playerValue)
		: shieldGeneratorAI(runtime::Ref<ShieldGeneratorAI>(shieldGeneratorAIValue)), player(runtime::Ref<Player>(playerValue)) {}
	~ShieldGeneratorAI_ItemUseObserver() override = default;
};

// Java ShieldGeneratorAI.java:57-60
bool ShieldGeneratorAI::canThink() {
	return false;
}

// Java ShieldGeneratorAI.java:62-69
void ShieldGeneratorAI::handleAttack(runtime::Ptr<Creature> creature) {
	GeneralNpcAI::handleAttack(creature);
	if (commons::utils::currentTimeMillis() - lastAttackedTime.get() > 10000) {
		lastAttackedTime = commons::utils::currentTimeMillis();
		shout(getAttackMsg());
	}
}

// Java ShieldGeneratorAI.java:71-73
void ShieldGeneratorAI::shout(network::aion::serverpackets::SM_SYSTEM_MESSAGE msg) {
	PacketSendUtility::broadcastToMap(getOwner(), msg);
}

// Java ShieldGeneratorAI.java:75-78
void ShieldGeneratorAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java ShieldGeneratorAI.java:80-98
bool ShieldGeneratorAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		if (chargeCount > 2) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_END());
			return false;
		}
		if (player.getInventory().getItemCountByItemId(164000289) == 0) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_NOITEM());
			return false;
		}
		if (isVortexSpawned.compareAndSet(false, true))
			handleVortexSpawn();
		if (isUnderCharge.compareAndSet(false, true))
			handleCharging(player);
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java ShieldGeneratorAI.java:100-126 (the finish task pins the AI, the player and the observer)
void ShieldGeneratorAI::handleCharging(Player& player) {
	const runtime::Ref<ShieldGeneratorAI_ItemUseObserver> observer = ShieldGeneratorAI_ItemUseObserver::create(*this, player);
	player.getObserveController()->attach(*observer);
	PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), 20000, 1));
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);
	shout(getChargeMsg());
	ShieldGeneratorAI_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ACTION_ITEM_NPC, ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver},
		[this, &player, &itemUseObserver] {
			PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::END_QUESTLOOT, 0, getObjectId()), true);
			PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), 20000, 2));
			player.getObserveController()->removeObserver(itemUseObserver);
			if (player.getInventory().decreaseByItemId(164000289, 1)) {
				handleChargeComplete();
				chargeCount++;
			}
			isUnderCharge.set(false);
		},
		20000));
}

// Java ShieldGeneratorAI.java:128-130
void ShieldGeneratorAI::deleteNpcs() {
	for (runtime::Ptr<Npc> s : charges)
		s->getController().delete_();
}

// Java ShieldGeneratorAI.java:132-136
void ShieldGeneratorAI::handleDespawned() {
	deleteNpcs();
	GeneralNpcAI::handleDespawned();
}

// Java ShieldGeneratorAI.java:138-144
void ShieldGeneratorAI::handleDied() {
	deleteNpcs();
	shout(getDestructionMsg());
	handleVortexDespawn();
	GeneralNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
