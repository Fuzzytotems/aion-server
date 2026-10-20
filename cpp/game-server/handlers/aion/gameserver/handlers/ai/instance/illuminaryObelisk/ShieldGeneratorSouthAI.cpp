#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorSouthAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(ShieldGeneratorSouthAI, "southern_generator");

// Java ShieldGeneratorSouthAI.java:17-20
SM_SYSTEM_MESSAGE ShieldGeneratorSouthAI::getAttackMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_03_ATTACKED();
}

// Java ShieldGeneratorSouthAI.java:22-25
SM_SYSTEM_MESSAGE ShieldGeneratorSouthAI::getChargeMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_03();
}

// Java ShieldGeneratorSouthAI.java:27-30
SM_SYSTEM_MESSAGE ShieldGeneratorSouthAI::getDestructionMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_03();
}

// Java ShieldGeneratorSouthAI.java:32-35
SM_SYSTEM_MESSAGE ShieldGeneratorSouthAI::getGateMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_03_BEGIN();
}

// Java ShieldGeneratorSouthAI.java:37-40 (Java's cast of the spawn to Npc: the charges are npcs)
void ShieldGeneratorSouthAI::handleChargeComplete() {
	charges.add(runtime::Ref<Npc>(*runtime::cast<Npc>(spawn(702224 + chargeCount, 298.304f, 254.48207f, 295.95157f, int8_t{0}))));
}

// Java ShieldGeneratorSouthAI.java:42-46. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-I2-3): the
// generator shouts its own getGateMsg(); Java shouted the east's STR_MSG_IDF5_U3_N_WAVE_01_BEGIN
void ShieldGeneratorSouthAI::handleVortexSpawn() {
	spawn(702016, 343.1202f, 254.10585f, 291.62302f, int8_t{0}, 34); // south
	shout(getGateMsg()); // the correction (was J-I2-3)
}

// Java ShieldGeneratorSouthAI.java:48-51
void ShieldGeneratorSouthAI::handleVortexDespawn() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({702016}))
		npc->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
