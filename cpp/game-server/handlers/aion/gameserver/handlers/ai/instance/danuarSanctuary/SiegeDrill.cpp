#include "aion/gameserver/handlers/ai/instance/danuarSanctuary/SiegeDrill.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {

AION_AI(SiegeDrill, "siege_drill");

using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_USE_OBJECT;

/**
 * Java: the anonymous ItemUseObserver of handleUseItemStart (SiegeDrill.java, fieldmap SiegeDrill$1): it aborts the use on any action of the
 * player. It captures the AI (Java's enclosing instance) and the player.
 */
struct SiegeDrill_ItemUseObserver final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<SiegeDrill> siegeDrill; // captured this
	const runtime::Ref<Player> player;         // captured param Player player

	static runtime::Ref<SiegeDrill_ItemUseObserver> create(SiegeDrill& siegeDrill, Player& player) {
		return runtime::makeRef<SiegeDrill_ItemUseObserver>(siegeDrill, player);
	}

	// Java SiegeDrill.java:67-74
	void abort() override {
		player->getController().cancelTask(TaskId::ACTION_ITEM_NPC);
		PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::END_QUESTLOOT, 0, siegeDrill->getObjectId()), true);
		PacketSendUtility::sendPacket(*player, SM_USE_OBJECT(player->getObjectId(), siegeDrill->getObjectId(), 0, siegeDrill->cancelBarAnimation));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	SiegeDrill_ItemUseObserver(SiegeDrill& siegeDrillValue, Player& playerValue)
		: siegeDrill(runtime::Ref<SiegeDrill>(siegeDrillValue)), player(runtime::Ref<Player>(playerValue)) {}
	~SiegeDrill_ItemUseObserver() override = default;
};

// Java SiegeDrill.java:38-41
float SiegeDrill::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java SiegeDrill.java:43-49
void SiegeDrill::handleDialogStart(Player& player) {
	if (isUsed.get()) {
		return;
	}
	handleUseItemStart(player);
}

// Java SiegeDrill.java:51-55
void SiegeDrill::handleMoveArrived() {
	NpcAI::handleMoveArrived();
	MoveEventHandler::onMoveArrived(*this);
}

// Java SiegeDrill.java:57-60
void SiegeDrill::handleAttack(runtime::Ptr<Creature> creature) {
	// do nothing
}

// Java SiegeDrill.java:62-89 (the finish task pins the AI, the player and the observer)
void SiegeDrill::handleUseItemStart(Player& player) {
	const int32_t delay = getTalkDelay();
	if (delay > 1) {
		const runtime::Ref<SiegeDrill_ItemUseObserver> observer = SiegeDrill_ItemUseObserver::create(*this, player);
		player.getObserveController()->attach(*observer);
		PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), getTalkDelay(), startBarAnimation));
		PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);
		SiegeDrill_ItemUseObserver& itemUseObserver = *observer;
		player.getController().addTask(TaskId::ACTION_ITEM_NPC, ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver},
			[this, &player, &itemUseObserver] {
				PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::END_QUESTLOOT, 0, getObjectId()), true);
				PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), getTalkDelay(), cancelBarAnimation));
				player.getObserveController()->removeObserver(itemUseObserver);
				handleUseItemFinish(player);
			},
			delay));
	} else {
		handleUseItemFinish(player);
	}
}

// Java SiegeDrill.java:91-113
void SiegeDrill::handleUseItemFinish(Player& player) {
	if (!player.getInventory().decreaseByItemId(185000174, 1)) {
		PacketSendUtility::broadcastToMap(getOwner(), 1401932);
		return;
	}
	if (isUsed.compareAndSet(false, true)) {
		getOwner().getSpawn()->setWalkerId("2A2D7F6EA351DCCEAA3CD097B9311ED308DCD2EC");
		WalkManager::startWalking(*this);
		getOwner().setState(CreatureState::ACTIVE, true);
		PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			runtime::Ptr<Npc> npc = getPosition()->getWorldMapInstance()->getNpc(233189);
			if (npc != nullptr) {
				getOwner().getSpawn()->setWalkerId(std::nullopt);
				WalkManager::stopWalking(*this);
				getOwner().setTarget(npc);
				skillengine::SkillEngine::getInstance().getSkill(getOwner(), 20778, 65, runtime::Ptr<VisibleObject>(npc))->useSkill();
				ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 5000);
			}
		}, 4800);
	}
}

// Java SiegeDrill.java:115-117
int32_t SiegeDrill::getTalkDelay() {
	return getObjectTemplate()->getTalkDelay() * 1000;
}

} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary
