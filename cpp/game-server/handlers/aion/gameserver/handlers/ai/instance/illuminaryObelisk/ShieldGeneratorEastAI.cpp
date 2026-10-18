#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorEastAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(ShieldGeneratorEastAI, "eastern_generator");

// Java ShieldGeneratorEastAI.java:17-20
SM_SYSTEM_MESSAGE ShieldGeneratorEastAI::getAttackMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_01_ATTACKED();
}

// Java ShieldGeneratorEastAI.java:22-25
SM_SYSTEM_MESSAGE ShieldGeneratorEastAI::getChargeMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_01();
}

// Java ShieldGeneratorEastAI.java:27-30
SM_SYSTEM_MESSAGE ShieldGeneratorEastAI::getDestructionMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_01();
}

// Java ShieldGeneratorEastAI.java:32-35
SM_SYSTEM_MESSAGE ShieldGeneratorEastAI::getGateMsg() {
	return SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_01_BEGIN();
}

// Java ShieldGeneratorEastAI.java:37-40 (Java's cast of the spawn to Npc: the charges are npcs)
void ShieldGeneratorEastAI::handleChargeComplete() {
	charges.add(runtime::Ref<Npc>(*runtime::cast<Npc>(spawn(702218 + chargeCount, 255.53876f, 297.46393f, 321.375f, int8_t{30}))));
}

// Java ShieldGeneratorEastAI.java:42-46
void ShieldGeneratorEastAI::handleVortexSpawn() {
	spawn(702014, 255.7926f, 338.22058f, 325.56473f, int8_t{0}, 60); // east
	shout(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_01_BEGIN());
}

// Java ShieldGeneratorEastAI.java:48-51
void ShieldGeneratorEastAI::handleVortexDespawn() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({702014}))
		npc->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
