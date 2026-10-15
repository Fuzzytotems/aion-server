#include "aion/gameserver/ai/manager/EmoteManager.h"

#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::ai::manager {

using model::EmotionType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

void EmoteManager::emoteStartAttacking(Npc& owner, Creature& target) {
	owner.unsetState(CreatureState::WALK_MODE);
	if (!owner.isInState(CreatureState::WEAPON_EQUIPPED)) {
		owner.setState(CreatureState::WEAPON_EQUIPPED);
		PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::CHANGE_SPEED, 0, target.getObjectId()));
		PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::ATTACKMODE_IN_MOVE, 0, target.getObjectId()));
	}
}

void EmoteManager::emoteStopAttacking(Npc& owner) {
	owner.unsetState(CreatureState::WEAPON_EQUIPPED);
	runtime::Ptr<VisibleObject> target = owner.getTarget();
	if (runtime::Ptr<Player> player = runtime::as<Player>(target)) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_UI_COMBAT_NPC_RETURN(owner.getObjectTemplate()->getL10n()));
	}
}

void EmoteManager::emoteStartFollowing(Npc& owner) {
	owner.unsetState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::CHANGE_SPEED, 0, 0));
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::NEUTRALMODE_IN_MOVE, 0, 0));
}

void EmoteManager::emoteStartWalking(Npc& owner) {
	owner.setState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::WALK));
}

void EmoteManager::emoteStopWalking(Npc& owner) {
	owner.unsetState(CreatureState::WALK_MODE);
}

void EmoteManager::emoteStartReturning(Npc& owner) {
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::CHANGE_SPEED, 0, 0));
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::NEUTRALMODE_IN_MOVE, 0, 0));
}

void EmoteManager::emoteStartIdling(Npc& owner) {
	owner.setState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::CHANGE_SPEED, 0, 0));
	PacketSendUtility::broadcastPacket(owner, SM_EMOTION(owner, EmotionType::NEUTRALMODE_IN_MOVE, 0, 0));
}

} // namespace aion::gameserver::ai::manager
