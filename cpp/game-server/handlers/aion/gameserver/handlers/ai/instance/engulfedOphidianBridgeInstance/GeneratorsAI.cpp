#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/GeneratorsAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

AION_AI(GeneratorsAI, "engulfedophidiangens");

// Java GeneratorsAI.java:21-75
void GeneratorsAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	switch (getOwner().getSpawn()->getStaticId()) {
		case 160:
			flag.set(runtime::cast<Npc>(spawn(802038, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // NORTH
			PacketSendUtility::broadcastToMap(getOwner(), 1402001);
			break;
		case 164:
			flag.set(runtime::cast<Npc>(spawn(802035, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // GUARD
			PacketSendUtility::broadcastToMap(getOwner(), 1402000);
			break;
		case 158:
			flag.set(runtime::cast<Npc>(spawn(802041, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // SOUTHERN
			PacketSendUtility::broadcastToMap(getOwner(), 1402002);
			break;
		case 162:
			flag.set(runtime::cast<Npc>(spawn(802044, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // Defence
			PacketSendUtility::broadcastToMap(getOwner(), 1402003);
			break;
		case 175:
			flag.set(runtime::cast<Npc>(spawn(802036, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // NORTH
			PacketSendUtility::broadcastToMap(getOwner(), 1401962);
			break;
		case 172:
			flag.set(runtime::cast<Npc>(spawn(802033, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // GUARD
			PacketSendUtility::broadcastToMap(getOwner(), 1401961);
			break;
		case 169:
			flag.set(runtime::cast<Npc>(spawn(802039, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // SOUTHERN
			PacketSendUtility::broadcastToMap(getOwner(), 1401963);
			break;
		case 171:
			flag.set(runtime::cast<Npc>(spawn(802042, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // Defence
			PacketSendUtility::broadcastToMap(getOwner(), 1401964);
			break;
		case 174:
			flag.set(runtime::cast<Npc>(spawn(802037, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // NORTH
			PacketSendUtility::broadcastToMap(getOwner(), 1401992);
			break;
		case 173:
			flag.set(runtime::cast<Npc>(spawn(802034, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // GUARD
			PacketSendUtility::broadcastToMap(getOwner(), 1401991);
			break;
		case 166:
			flag.set(runtime::cast<Npc>(spawn(802040, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // SOUTHERN
			PacketSendUtility::broadcastToMap(getOwner(), 1401993);
			break;
		case 170:
			flag.set(runtime::cast<Npc>(spawn(802043, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}))); // Defence
			PacketSendUtility::broadcastToMap(getOwner(), 1401994);
			break;
	}
}

// Java GeneratorsAI.java:77-82
void GeneratorsAI::handleDespawned() {
	ActionItemNpcAI::handleDespawned();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

// Java GeneratorsAI.java:84-89
void GeneratorsAI::handleDied() {
	ActionItemNpcAI::handleDied();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
