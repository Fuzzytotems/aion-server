#include "aion/gameserver/handlers/ai/DeliveryManAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai {

AION_AI(DeliveryManAI, "deliveryman");

// Java DeliveryManAI.java:29-40. The despawn lambda (a task, pin {this}) is pinned on this AI, a part of its npc.
void DeliveryManAI::handleSpawned() {
	FollowingNpcAI::handleSpawned();
	runtime::Ptr<Player> mailOwner = getPlayer();
	getOwner().getController().addTask(model::TaskId::DESPAWN,
		ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, SERVICE_TIME));
	if (mailOwner == nullptr) {
		// Java: handleFollowMe(null) - FollowEventHandler.follow sets the null target and emotes - then handleCreatureMoved(null) throws
		// NullPointerException on creature.equals(...)
		if (setStateIfNot(AIState::FOLLOWING)) {
			getOwner().setTarget(nullptr);
			gameserver::ai::manager::EmoteManager::emoteStartFollowing(getOwner());
		}
		throw runtime::NullPointerException("DeliveryManAI.getPlayer()");
	}
	handleFollowMe(*mailOwner);
	handleCreatureMoved(*mailOwner);
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390266, 1500);  // Here is your mail, akakak!
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390268, 30000); // Time is silver my friend, akakak!
}

// Java DeliveryManAI.java:42-49
void DeliveryManAI::handleDespawned() {
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390267); // Whiririkk, let's go!
	runtime::Ptr<Player> player = World::getInstance().getPlayer(getOwner().getCreatorId());
	if (player != nullptr) {
		runtime::Ptr<Npc> postman = player->getPostman();
		if (postman != nullptr && getOwner().equals(*postman))
			player->setPostman(nullptr);
	}
	FollowingNpcAI::handleDespawned();
}

// Java DeliveryManAI.java:51-58
void DeliveryManAI::handleDialogStart(Player& player) {
	runtime::Ptr<Player> mailOwner = getPlayer();
	if (mailOwner != nullptr && player.equals(*mailOwner)) {
		player.getMailbox()->mailBoxState.set(PlayerMailboxState::EXPRESS);
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::MAIL)));
	} else {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390269); // There is no mail for you, nyerk.
	}
}

// Java DeliveryManAI.java:60-62
runtime::Ptr<Player> DeliveryManAI::getPlayer() {
	runtime::Ptr<world::WorldMapInstance> instance = getOwner().getPosition()->getWorldMapInstance();
	if (instance == nullptr) // Java: getWorldMapInstance().getPlayer(...) on null
		throw runtime::NullPointerException("WorldPosition.getWorldMapInstance()");
	return instance->getPlayer(getOwner().getCreatorId());
}

} // namespace aion::gameserver::handlers::ai
