#include "aion/gameserver/handlers/instance/KromedesTrialInstance.h"

#include <string>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h" // the Ref<Effect> applyEffectDirectly returns
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(KromedesTrialInstance, 300230000);

// Java KromedesTrialInstance.java:38-40
KromedesTrialInstance::KromedesTrialInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<KromedesTrialInstance> KromedesTrialInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<KromedesTrialInstance>(instance);
}

// Java KromedesTrialInstance.java:42-47
void KromedesTrialInstance::onEnterInstance(Player& player) {
	skillId = player.getRace() == Race::ASMODIANS ? 19270 : 19220;
	sendMovie(player, 453);
	SkillEngine::getInstance().applyEffectDirectly(skillId.get(), player, player);
}

// Java KromedesTrialInstance.java:49-74
void KromedesTrialInstance::onEnterZone(Player& player, ZoneInstance& zone) {
	const std::string zoneName = zone.getAreaTemplate()->getZoneName()->toString();
	if (zoneName == "MANOR_ENTRANCE_300230000") {
		sendMovie(player, 462);
	} else if (zoneName == "KALIGA_LIBRARY_300230000") {
		PacketSendUtility::sendMonologue(player, 1111370); // The door to the Kaliga Treasury should be around here somewhere....
	} else if (zoneName == "KALIGA_TREASURY_300230000") {
		if (isBossSpawned.compareAndSet(false, true)) {
			runtime::Ptr<Npc> wyr = instance->getNpc(217002);
			runtime::Ptr<Npc> angerr = instance->getNpc(217000);
			runtime::Ptr<Npc> hamam = instance->getNpc(216982);
			if (isDead(wyr) && isDead(angerr) && isDead(hamam)) {
				spawn(217005, 669.214f, 774.387f, 216.88f, int8_t{60});
				spawn(217001, 663.8805f, 779.1967f, 216.26213f, int8_t{60});
				spawn(217003, 663.0468f, 774.6116f, 216.26215f, int8_t{60});
				spawn(217004, 663.0468f, 770.03815f, 216.26212f, int8_t{60});
			} else {
				spawn(217006, 669.214f, 774.387f, 216.88f, int8_t{60});
			}
		}
	}
}

// Java KromedesTrialInstance.java:76-86
void KromedesTrialInstance::onPlayMovieEnd(Player& player, int32_t movieId) {
	if (movieId == 454) {
		runtime::Ptr<Npc> magasPotion = instance->getNpc(730308);
		if (magasPotion != nullptr && PositionUtil::isInRange(player, *magasPotion, 20)) {
			int32_t relicKeyId = 185000109;
			player.getInventory().decreaseByItemId(relicKeyId, player.getInventory().getItemCountByItemId(relicKeyId));
			TeleportService::teleportTo(player, *instance, 687.56116f, 681.68225f, 200.28648f, int8_t{30});
		}
	}
}

// Java KromedesTrialInstance.java:88-105
void KromedesTrialInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 216968: // Divine Hisen
			isPlayerInManor = true;
			break;
		case 282093: // Mana Relic
			removeKaligaBuff(19248);
			scheduleRespawn(npc);
			npc.getController().delete_();
			break;
		case 282095: // Strength Relic
			removeKaligaBuff(19247);
			scheduleRespawn(npc);
			npc.getController().delete_();
			break;
		default:
			break;
	}
}

// Java KromedesTrialInstance.java:107-118
bool KromedesTrialInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, true, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	if (!isPlayerInManor.get())
		TeleportService::teleportTo(player, *instance, 248.0f, 244.0f, 189.0f);
	else
		TeleportService::teleportTo(player, *instance, 686.0f, 676.0f, 201.0f);
	SkillEngine::getInstance().applyEffectDirectly(skillId.get(), player, player);
	return true;
}

// Java KromedesTrialInstance.java:120-125
void KromedesTrialInstance::removeKaligaBuff(int32_t skillId) {
	runtime::Ptr<Npc> kaliga = instance->getNpc(217006);
	if (kaliga != nullptr && !kaliga->isDead()) {
		kaliga->getEffectController()->removeEffect(skillId);
	}
}

// Java KromedesTrialInstance.java:127-138 (the lambda keeps the dead relic alive, as Java's capture does)
void KromedesTrialInstance::scheduleRespawn(Npc& npc) {
	runtime::Ref<Npc> relic(npc);
	ThreadPoolManager::getInstance().schedule({this}, [this, relic] {
		runtime::Ptr<Npc> kaliga = instance->getNpc(217006);
		if (kaliga != nullptr && !kaliga->isDead()) {
			runtime::Ref<SpawnTemplate> npcST = relic->getSpawn();
			runtime::Ref<SpawnTemplate> newST = spawnengine::SpawnEngine::newSingleTimeSpawn(npcST->getWorldId(), relic->getNpcId(), npcST->getX(),
				npcST->getY(), npcST->getZ(), npcST->getHeading());
			newST->setStaticId(npcST->getStaticId());
			spawnengine::SpawnEngine::spawnObject(*newST, instance->getInstanceId());
		}
	}, 10000);
}

// Java KromedesTrialInstance.java:140-142
bool KromedesTrialInstance::isDead(runtime::Ptr<Npc> npc) {
	return npc == nullptr || npc->isDead();
}

// Java KromedesTrialInstance.java:144-149
void KromedesTrialInstance::sendMovie(Player& player, int32_t movieId) {
	if (!sentMovies.contains(movieId)) {
		sentMovies.add(movieId);
		PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, movieId, true));
	}
}

// Java KromedesTrialInstance.java:151-157
bool KromedesTrialInstance::isRestrictedToInstance(Item& item) {
	switch (item.getItemId()) {
		case 185000098:
		case 185000099:
		case 185000100:
		case 185000109:
			return true; // Temple Vault Door Key, Dungeon Grate Key, Dungeon Door Key, Relic Key
		default:
			return GeneralInstanceHandler::isRestrictedToInstance(item);
	}
}

} // namespace aion::gameserver::handlers::instance
