#include "aion/gameserver/handlers/ai/instance/kamarBf/ReianSupplyItemsAI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

AION_AI(ReianSupplyItemsAI, "reiansupplyitems");

// Java ReianSupplyItemsAI.java:20-24
void ReianSupplyItemsAI::handleSpawned() {
	ChestAI::handleSpawned();
	flag.set(runtime::cast<Npc>(spawn(801959, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
}

// Java ReianSupplyItemsAI.java:26-31
void ReianSupplyItemsAI::handleDespawned() {
	ChestAI::handleDespawned();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

// Java ReianSupplyItemsAI.java:33-38
void ReianSupplyItemsAI::handleDied() {
	ChestAI::handleDied();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
