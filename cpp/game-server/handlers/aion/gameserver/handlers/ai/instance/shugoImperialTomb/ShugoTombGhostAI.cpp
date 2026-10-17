#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombGhostAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombGhostAI, "shugo_tomb_ghost");

// Java ShugoTombGhostAI.java:23-26
bool ShugoTombGhostAI::canThink() {
	return false;
}

// Java ShugoTombGhostAI.java:28-33
void ShugoTombGhostAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	getOwner().setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

// Java ShugoTombGhostAI.java:35-44
bool ShugoTombGhostAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_DECAY:
			switch (getNpcId()) {
				case 219505:
				case 219506:
				case 219507:
					return false;
				default:
					return true;
			}
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
