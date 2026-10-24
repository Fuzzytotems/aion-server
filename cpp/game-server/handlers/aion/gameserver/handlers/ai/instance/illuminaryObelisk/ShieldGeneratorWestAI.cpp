#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorWestAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(ShieldGeneratorWestAI, "western_generator");

// Java ShieldGeneratorWestAI.java:17-20
SM_SYSTEM_MESSAGE ShieldGeneratorWestAI::getAttackMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_02_ATTACKED();
}

// Java ShieldGeneratorWestAI.java:22-25
SM_SYSTEM_MESSAGE ShieldGeneratorWestAI::getChargeMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_02();
}

// Java ShieldGeneratorWestAI.java:27-30
SM_SYSTEM_MESSAGE ShieldGeneratorWestAI::getDestructionMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_02();
}

// Java ShieldGeneratorWestAI.java:32-35
SM_SYSTEM_MESSAGE ShieldGeneratorWestAI::getGateMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_02_BEGIN();
}

// Java ShieldGeneratorWestAI.java:37-40 (Java's cast of the spawn to Npc: the charges are npcs)
void ShieldGeneratorWestAI::handleChargeComplete() {
	charges.add(runtime::Ref<Npc>(*runtime::cast<Npc>(spawn(702221 + chargeCount, 255.38824f, 211.9726f, 321.37753f, int8_t{90}))));
}

// Java ShieldGeneratorWestAI.java:42-46. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-I2-3): the
// generator shouts its own getGateMsg(); Java shouted the east's STR_MSG_IDF5_U3_N_WAVE_01_BEGIN
void ShieldGeneratorWestAI::handleVortexSpawn() {
	spawn(702015, 255.7034f, 171.83853f, 325.81653f, int8_t{0}, 18); // west
	shout(getGateMsg()); // the correction (was J-I2-3)
}

// Java ShieldGeneratorWestAI.java:48-51
void ShieldGeneratorWestAI::handleVortexDespawn() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({702015}))
		npc->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
