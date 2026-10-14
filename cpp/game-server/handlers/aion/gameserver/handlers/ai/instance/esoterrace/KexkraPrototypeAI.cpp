#include "aion/gameserver/handlers/ai/instance/esoterrace/KexkraPrototypeAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

AION_AI(KexkraPrototypeAI, "kexkraprototype");

// Java KexkraPrototypeAI.java:26-30
void KexkraPrototypeAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java KexkraPrototypeAI.java:32-40
void KexkraPrototypeAI::handleHpPhase(int32_t /*phaseHpPercent*/) {
	getKnownList().forEachPlayer([](Player& player) {
		if (!player.isDead()) {
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLAY_MOVIE(false, 0, 0, 472, true));
		}
	});
	spawn(217206, 1320.639282f, 1171.063354f, 51.494003f, int8_t{0});
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
