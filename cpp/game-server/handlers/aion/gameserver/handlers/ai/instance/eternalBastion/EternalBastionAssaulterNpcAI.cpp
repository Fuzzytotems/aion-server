#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAssaulterNpcAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionAssaulterNpcAI, "eternal_bastion_assaulter");

// Java EternalBastionAssaulterNpcAI.java:23-28
int32_t EternalBastionAssaulterNpcAI::getAggroRange() {
	int32_t range = getOwner().getAggroRange();
	if (range < 11)
		range = 11;
	return range;
}

// Java EternalBastionAssaulterNpcAI.java:30-34
void EternalBastionAssaulterNpcAI::handleSpawned() {
	EternalBastionAggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { startWalking(); }, 3000);
}

// Java EternalBastionAssaulterNpcAI.java:36-40
void EternalBastionAssaulterNpcAI::startWalking() {
	gameserver::ai::manager::WalkManager::startWalking(*this);
	getOwner().unsetState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

// Java EternalBastionAssaulterNpcAI.java:42-48
bool EternalBastionAssaulterNpcAI::isDestinationReached() {
	getOwner().unsetState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
	hateCommander();
	return EternalBastionAggressiveNpcAI::isDestinationReached();
}

// Java EternalBastionAssaulterNpcAI.java:50-56
void EternalBastionAssaulterNpcAI::hateCommander() {
	runtime::Ptr<Npc> commander = getPosition()->getWorldMapInstance()->getNpc(209516);
	if (commander == nullptr)
		commander = getPosition()->getWorldMapInstance()->getNpc(209517);
	if (commander != nullptr && !getOwner().getAggroList().isHating(*commander) && PositionUtil::isInRange(getOwner(), *commander, 20))
		getOwner().getAggroList().addHate(*commander, 100000);
}

// Java EternalBastionAssaulterNpcAI.java:58-62
void EternalBastionAssaulterNpcAI::handleBackHome() {
	EternalBastionAggressiveNpcAI::handleBackHome();
	hateCommander();
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
