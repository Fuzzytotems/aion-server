#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombModoAI.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombModoAI, "shugo_tomb_modo");

// Java ShugoTombModoAI.java:16-20
void ShugoTombModoAI::handleSpawned() {
	ShugoTombAttackerAI::handleSpawned();
	handleHate();
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
