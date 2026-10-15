#include "aion/gameserver/handlers/instance/TalocsHollowInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(TalocsHollowInstance, 300190000);

// Java TalocsHollowInstance.java:40-42
TalocsHollowInstance::TalocsHollowInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<TalocsHollowInstance> TalocsHollowInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<TalocsHollowInstance>(instance);
}

// Java TalocsHollowInstance.java:44-47
void TalocsHollowInstance::onEnterInstance(Player& player) {
	addItems(player);
}

// Java TalocsHollowInstance.java:49-56
void TalocsHollowInstance::addItems(Player& player) {
	runtime::Ptr<QuestState> qs1 = player.getQuestStateList()->getQuestState(10032);
	runtime::Ptr<QuestState> qs2 = player.getQuestStateList()->getQuestState(20032);
	if ((qs1 != nullptr && qs1->getStatus() == QuestStatus::START) || (qs2 != nullptr && qs2->getStatus() == QuestStatus::START))
		return;
	addMissingItems(player, 164000099); // Taloc's Tears
	addMissingItems(player, player.getRace() == Race::ELYOS ? 160001286 : 160001287); // Taloc Fruit
}

// Java TalocsHollowInstance.java:58-61
void TalocsHollowInstance::addMissingItems(Player& player, int32_t itemId) {
	if (player.getInventory().getFirstItemByItemId(itemId) == nullptr)
		ItemService::addItem(player, itemId, 1);
}

// Java TalocsHollowInstance.java:63-75
bool TalocsHollowInstance::isRestrictedToInstance(Item& item) {
	switch (item.getItemId()) {
		case 164000099: // Taloc's Tears
		case 164000137: // Shishir's Powerstone
		case 164000138: // Gellmar's Wardstone
		case 164000139: // Neith's Sleepstone
		case 185000088: // Shishir's Corrosive Fluid
		case 185000108: // Dorkin's Pocket Knife
			return true;
		default:
			break;
	}
	return GeneralInstanceHandler::isRestrictedToInstance(item);
}

// Java TalocsHollowInstance.java:77-81
void TalocsHollowInstance::onAggro(Npc& npc) {
	if (npc.getNpcId() == 215480 && isQueenMosquaHome.compareAndSet(true, false))
		instance->setDoorState(7, false);
}

// Java TalocsHollowInstance.java:83-89
void TalocsHollowInstance::onBackHome(Npc& npc) {
	if (npc.getNpcId() == 215480) { // queen mosqua
		isQueenMosquaHome.set(true);
		instance->setDoorState(7, true);
	}
}

// Java TalocsHollowInstance.java:91-138
void TalocsHollowInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 215467: // kinquid
			instance->setDoorState(48, true);
			instance->setDoorState(49, true);
			break;
		case 215457: // ancient octanus
			deleteAliveNpcs({700633});
			break;
		case 215480: { // queen mosqua
			instance->setDoorState(7, true);
			runtime::Ptr<Npc> insectEgg = getNpc(700738);
			if (insectEgg != nullptr) {
				insectEgg->getController().delete_();
				runtime::Ref<SpawnTemplate> eggTemplate = insectEgg->getSpawn();
				spawn(700739, eggTemplate->getX(), eggTemplate->getY(), eggTemplate->getZ(), eggTemplate->getHeading(), 11);
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDELIM_EGG_BREAK());
				instance->forEachPlayer([](Player& player) {
					runtime::Ptr<Summon> summon = player.getSummon();
					if (summon != nullptr) {
						if (summon->getNpcId() == 799500 || summon->getNpcId() == 799501) {
							SummonsService::release(*summon, UnsummonType::UNSPECIFIED);
							PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, 435, true));
						}
					}
				});
			}
			break;
		}
		case 700739: { // cracked huge insect egg
			runtime::Ref<SpawnTemplate> crackedEggTemplate = npc.getSpawn();
			spawn(281817, crackedEggTemplate->getX(), crackedEggTemplate->getY(), crackedEggTemplate->getZ(), crackedEggTemplate->getHeading(), 9);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDELIM_WIND_INFO());
			break;
		}
		case 215488: { // celestius
			runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage();
			if (player != nullptr)
				PacketSendUtility::sendPacket(*player, SM_PLAY_MOVIE(false, 0, 10021, 437, true));
			runtime::Ptr<Npc> contaminatedFragment = getNpc(700740);
			if (contaminatedFragment != nullptr) {
				runtime::Ref<SpawnTemplate> fragmentTemplate = contaminatedFragment->getSpawn();
				spawn(700741, fragmentTemplate->getX(), fragmentTemplate->getY(), fragmentTemplate->getZ(), fragmentTemplate->getHeading(), 92);
				contaminatedFragment->getController().delete_();
			}
			spawn(799503, 548.0f, 811.0f, 1375.0f, int8_t{0});
			break;
		}
		default:
			break;
	}
}

// Java TalocsHollowInstance.java:140-152
void TalocsHollowInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 700940:
			player->getLifeStats()->increaseHp(SM_ATTACK_STATUS::TYPE::HP, 20000, npc);
			npc.getController().delete_();
			break;
		case 700941:
			player->getLifeStats()->increaseHp(SM_ATTACK_STATUS::TYPE::HP, 30000, npc);
			npc.getController().delete_();
			break;
		default:
			break;
	}
}

// Java TalocsHollowInstance.java:154-159
void TalocsHollowInstance::sendMovie(Player& player, int32_t movie) {
	if (!movies.contains(movie)) {
		movies.add(movie);
		PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, movie, true));
	}
}

// Java TalocsHollowInstance.java:161-166
void TalocsHollowInstance::onInstanceCreate() {
	instance->setDoorState(48, true);
	instance->setDoorState(7, true);
	spawnRings();
}

// Java TalocsHollowInstance.java:168-175 (the rings' templates are immortal, as static data: FlyRingTemplate.h)
void TalocsHollowInstance::spawnRings() {
	runtime::Ref<Point3D> c1 = Point3D::create(253.85039, 649.23535, 1171.8772);
	runtime::Ref<Point3D> a1 = Point3D::create(253.85039, 649.23535, 1177.8772);
	runtime::Ref<Point3D> b1 = Point3D::create(262.84872, 649.4091, 1171.8772);
	runtime::Ref<FlyRing> f1 = VisibleObject::create<FlyRing>(new FlyRingTemplate("TALOCS_1", mapId, *c1, *a1, *b1, 8), instance->getInstanceId());
	f1->spawn();
	runtime::Ref<Point3D> c2 = Point3D::create(592.32275, 844.056, 1295.0966);
	runtime::Ref<Point3D> a2 = Point3D::create(592.32275, 844.056, 1301.0966);
	runtime::Ref<Point3D> b2 = Point3D::create(595.2305, 835.5387, 1295.0966);
	runtime::Ref<FlyRing> f2 = VisibleObject::create<FlyRing>(new FlyRingTemplate("TALOCS_2", mapId, *c2, *a2, *b2, 8), instance->getInstanceId());
	f2->spawn();
}

// Java TalocsHollowInstance.java:177-185
bool TalocsHollowInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "TALOCS_1") {
		sendMovie(player, 463);
	} else if (flyingRing == "TALOCS_2") {
		sendMovie(player, 464);
	}
	return false;
}

// Java TalocsHollowInstance.java:187-194
bool TalocsHollowInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	TeleportService::teleportTo(player, *instance, 202.26694f, 226.0532f, 1098.236f, int8_t{30});
	return true;
}

} // namespace aion::gameserver::handlers::instance
