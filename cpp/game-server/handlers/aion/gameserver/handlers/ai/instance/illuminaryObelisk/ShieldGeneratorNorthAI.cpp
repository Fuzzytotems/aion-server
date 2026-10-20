#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorNorthAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(ShieldGeneratorNorthAI, "northern_generator");

// Java ShieldGeneratorNorthAI.java:17-20
SM_SYSTEM_MESSAGE ShieldGeneratorNorthAI::getAttackMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_04_ATTACKED();
}

// Java ShieldGeneratorNorthAI.java:22-25
SM_SYSTEM_MESSAGE ShieldGeneratorNorthAI::getChargeMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_04();
}

// Java ShieldGeneratorNorthAI.java:27-30
SM_SYSTEM_MESSAGE ShieldGeneratorNorthAI::getDestructionMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_04();
}

// Java ShieldGeneratorNorthAI.java:32-35
SM_SYSTEM_MESSAGE ShieldGeneratorNorthAI::getGateMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_04_BEGIN();
}

// Java ShieldGeneratorNorthAI.java:37-40 (Java's cast of the spawn to Npc: the charges are npcs)
void ShieldGeneratorNorthAI::handleChargeComplete() {
	charges.add(runtime::Ref<Npc>(*runtime::cast<Npc>(spawn(702227 + chargeCount, 212.64922f, 254.5639f, 295.94763f, int8_t{60}))));
}

// Java ShieldGeneratorNorthAI.java:42-46
void ShieldGeneratorNorthAI::handleVortexSpawn() {
	spawn(702017, 169.5563f, 254.52907f, 293.04276f, int8_t{0}, 17); // north
	shout(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_04_BEGIN());
}

// Java ShieldGeneratorNorthAI.java:48-51
void ShieldGeneratorNorthAI::handleVortexDespawn() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({702017}))
		npc->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
