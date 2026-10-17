#include "aion/gameserver/handlers/instance/BeshmundirInstance.h"

#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(BeshmundirInstance, 300170000);

// Java BeshmundirInstance.java:32-34
BeshmundirInstance::BeshmundirInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<BeshmundirInstance> BeshmundirInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<BeshmundirInstance>(instance);
}

// Java BeshmundirInstance.java:36-40
void BeshmundirInstance::onEnterInstance(Player& player) {
	if (!instanceRace.get().has_value())
		instanceRace = player.getRace();
}

// Java BeshmundirInstance.java:42-45
float BeshmundirInstance::getExpMultiplier() {
	return 3.0f;
}

// Java BeshmundirInstance.java:47-140
void BeshmundirInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 216583: // Brutal Soulwatcher (Difficult)
		case 216587: // Brutal Soulwatcher (Normal)
			spawn(799518, 936.0029f, 441.51712f, 220.5029f, int8_t{28});
			break;
		case 216584: // Brutal Soulwatcher (Difficult)
		case 216588: // Brutal Soulwatcher (Normal)
			spawn(799519, 791.0439f, 439.79608f, 220.3506f, int8_t{28});
			break;
		case 216585: // Brutal Soulwatcher (Difficult)
		case 216589: // Brutal Soulwatcher (Normal)
			spawn(799520, 820.70624f, 278.828f, 220.19385f, int8_t{55});
			break;
		case 216586: // Temadaro (Difficult)
		case 216590: { // Temadaro (Normal)
			int32_t killedCount = macunbello.getAndSet(0);
			if (killedCount < 12) {
				// strongest macunbello
				runtime::Ptr<Npc> npcMacunbello = runtime::cast<Npc>(spawn(216735, 981.015015f, 134.373001f, 241.755005f, int8_t{30}));
				SkillEngine::getInstance().applyEffectDirectly(19046, *npcMacunbello, *npcMacunbello);
			} else if (killedCount < 14) {
				// 2nd strongest macunbello
				runtime::Ptr<Npc> npcMacunbello = runtime::cast<Npc>(spawn(216734, 981.015015f, 134.373001f, 241.755005f, int8_t{30}));
				SkillEngine::getInstance().applyEffectDirectly(19047, *npcMacunbello, *npcMacunbello);
			} else if (killedCount < 21) {
				// 2nd weakest macunbello
				runtime::Ptr<Npc> npcMacunbello = runtime::cast<Npc>(spawn(216737, 981.015015f, 134.373001f, 241.755005f, int8_t{30}));
				SkillEngine::getInstance().applyEffectDirectly(19048, *npcMacunbello, *npcMacunbello);
			} else {
				spawn(216245, 981.015015f, 134.373001f, 241.755005f, int8_t{30}); // weakest macunbello
			}
			sendPacket(SM_QUEST_ACTION(0, 0));
			instance->setDoorState(467, true);
			break;
		}
		case 799342:
			sendPacket(network::aion::serverpackets::SM_PLAY_MOVIE(false, 0, 0, 447, true));
			break;
		case 216157:
		case 216238:
			instance->setDoorState(470, true);
			spawn(216159, 1357.0598f, 388.6637f, 249.26372f, int8_t{90});
			break;
		case 216165:
		case 216246:
			instance->setDoorState(473, true);
			break;
		case 216739:
		case 216740: {
			int32_t killedTotal = kills.incrementAndGet();
			if (killedTotal < 10) {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdSpecter_Spawn());
			} else if (killedTotal == 10) {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdSpecter_Start());
				spawn(216158, 1356.5719f, 147.76418f, 246.27373f, int8_t{91});
			}
			break;
		}
		case 216158:
		case 216239:
			instance->setDoorState(471, true);
			break;
		case 700608:
			if (instanceRace.get() == Race::ASMODIANS) {
				spawn(799342, 1357.1f, 76.044f, 248.595f, int8_t{0});
			}
			break;
		case 216263:
		case 216182:
			// this is a safety Mechanism
			// super boss
			spawn(216183, 558.306f, 1369.02f, 224.795f, int8_t{70});
			// gate
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_BigOrb_Spawn());
			spawn(730275, 1611.1266f, 1604.6935f, 310.39972f, int8_t{17}, 426);
			break;
		case 216250: // Dorakiki the Bold
		case 216169:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdShulack_Rufukin());
			spawn(216527, 1161.859985f, 1213.859985f, 284.057007f, int8_t{110}); // Lupukin: cat trader
			break;
		case 216206:
		case 216207:
		case 216208:
		case 216209:
		case 216210:
		case 216211:
		case 216212:
		case 216213:
			switch (macunbello.incrementAndGet()) {
				case 12:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdLich_weakness1());
					break;
				case 14:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdLich_weakness2());
					break;
				case 21:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdLich_weakness3());
					break;
			}
			break;
	}
}

// Java BeshmundirInstance.java:142-144
void BeshmundirInstance::sendPacket(network::aion::AionServerPacket&& packet) {
	PacketSendUtility::broadcastToMap(*instance, std::move(packet));
}

// Java BeshmundirInstance.java:146-150
void BeshmundirInstance::onPlayMovieEnd(Player& player, int32_t movieId) {
	if (movieId == 443)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_BigOrb_Spawn());
}

// Java BeshmundirInstance.java:152-155
void BeshmundirInstance::onInstanceCreate() {
	instance->setDoorState(535, true);
}

} // namespace aion::gameserver::handlers::instance
