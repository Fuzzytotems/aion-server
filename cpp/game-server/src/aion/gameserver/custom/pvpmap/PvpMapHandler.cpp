#include "aion/gameserver/custom/pvpmap/PvpMapHandler.h"

#include <cmath>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/PvpService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/BindPointTeleportService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/StaticDoorSpawnManager.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::custom::pvpmap {

using configs::main::CustomConfig;
using model::ChatType;
using model::TaskId;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_BIND_POINT_TELEPORT;
using network::aion::serverpackets::SM_MESSAGE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;
using utils::ThreadPoolManager;
using world::WorldPosition;

namespace {

/** Java: Rnd.get(list) - an element of the list's snapshot; nextInt(0)'s IllegalArgumentException for an empty one */
runtime::Ptr<WorldPosition> randomPosition(const runtime::ArrayList<runtime::Ref<WorldPosition>>& list) {
	std::vector<runtime::Ptr<WorldPosition>> positions;
	for (runtime::Ptr<WorldPosition> position : list)
		positions.push_back(position);
	if (positions.empty())
		throw runtime::IllegalArgumentException("bound must be positive");
	return positions[static_cast<size_t>(commons::utils::Rnd::nextInt(static_cast<int32_t>(positions.size())))];
}

} // namespace

// fieldmap-class: com.aionemu.gameserver.custom.pvpmap.PvpMapHandler$1
/** Java: the anonymous ItemUseObserver of getAllObserver: an abort cancels the bind point teleport */
struct PvpMapHandler_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<Player> p; // captured param Player p

	static runtime::Ref<PvpMapHandler_ItemUseObserver> create(Player& player) { return runtime::makeRef<PvpMapHandler_ItemUseObserver>(player); }

	void abort() override { services::teleport::BindPointTeleportService::cancelTeleport(*p, 1); }

protected:
	explicit PvpMapHandler_ItemUseObserver(Player& player) : p(player) {}
	~PvpMapHandler_ItemUseObserver() override = default;
};

// Java PvpMapHandler.java:62-64
PvpMapHandler::PvpMapHandler(world::WorldMapInstance& instanceValue) : GeneralInstanceHandler(instanceValue) {
}

PvpMapHandler::~PvpMapHandler() = default;

runtime::Ref<PvpMapHandler> PvpMapHandler::create(world::WorldMapInstance& instanceValue) {
	return runtime::makeRef<PvpMapHandler>(instanceValue);
}

// Java PvpMapHandler.java:66-76
void PvpMapHandler::onInstanceCreate() {
	spawnengine::StaticDoorSpawnManager::spawnTemplate(*instance);
	instance->forEachDoor([](model::gameobjects::StaticDoor& door) { door.setOpen(true); });
	addRespawnLocations();
	startSupplyTask();
	spawnKeymasters();
	spawnTreasureChests();
	spawnNpcs();
	startRandomBossTask();
}

// Java PvpMapHandler.java:78-93
void PvpMapHandler::spawnShugo(Player& player) {
	if (CustomConfig::PVP_MAP_ENABLED.load() && commons::utils::Rnd::chance() < SHUGO_SPAWN_RATE) {
		deleteAliveNpcs({833543});
		double radian = utils::PositionUtil::convertHeadingToAngle(player.getHeading()) * 0.017453292519943295; // Java: Math.toRadians (DEGREES_TO_RADIANS)
		float x = player.getX() + static_cast<float>(std::cos(radian) * 2);
		float y = player.getY() + static_cast<float>(std::sin(radian) * 2);
		float z = world::geo::GeoService::getInstance().getZ(player.getWorldId(), x, y, player.getZ(), instance->getInstanceId());
		if (std::isnan(z))
			z = player.getZ() + 0.5f;
		int8_t heading = utils::PositionUtil::getHeadingTowards(x, y, player.getX(), player.getY());
		runtime::Ref<model::templates::spawns::SpawnTemplate> spawnTemplate =
			spawnengine::SpawnEngine::newSingleTimeSpawn(mapId, 833543, x, y, z, heading, nullptr, "customcdreset");
		spawnengine::SpawnEngine::spawnObject(*spawnTemplate, instance->getInstanceId());
	}
}

// Java PvpMapHandler.java:96-98: spawns a supply chest every ~6min if there are enough players on the map
void PvpMapHandler::startSupplyTask() {
	supplyTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { scheduleSupplySpawn(); }, 120000, 400000));
}

// Java PvpMapHandler.java:100-114
void PvpMapHandler::scheduleSupplySpawn() {
	if (spawnAllowed()) {
		if (supplyPositions.isEmpty()) {
			addSupplyPositions();
		}
		runtime::Ref<WorldPosition> pos(*randomPosition(supplyPositions));
		supplyPositions.remove(pos);
		spawn(831980, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading()); // flag
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_GUARDLIGHTHERO_SPAWN_IDLDF5_UNDER_01_WAR(), 0);
		scheduleSupplyDespawn();
		ThreadPoolManager::getInstance().schedule({this}, [this, pos] {
			spawn(233192, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading()); // chest
		}, 30000);
	}
}

// Java PvpMapHandler.java:116-122
void PvpMapHandler::spawnKeymasters() {
	addKeymasterPositions();
	const int32_t npcIds[] = {219218, 219218, 219218, 219191, 219191, 219192, 219192, 219193};
	for (int32_t id : npcIds) {
		spawnKeymasterOrTreasureChest(id, true);
	}
}

// Java PvpMapHandler.java:124-134
void PvpMapHandler::spawnKeymasterOrTreasureChest(int32_t npcId, bool isKeymaster) {
	runtime::Ref<WorldPosition> pos;
	if (isKeymaster) {
		pos = runtime::Ref<WorldPosition>(*randomPosition(keymasterPositions));
		keymasterPositions.remove(pos);
	} else {
		pos = runtime::Ref<WorldPosition>(*randomPosition(treasurePositions));
		treasurePositions.remove(pos);
	}
	spawn(npcId, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading());
}

// Java PvpMapHandler.java:136-138
void PvpMapHandler::scheduleRespawn(int32_t npcId, int32_t time, bool isKeymaster) {
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this, npcId, isKeymaster] { spawnKeymasterOrTreasureChest(npcId, isKeymaster); }, time,
		utils::TimeUnit::MINUTES));
}

// Java PvpMapHandler.java:140-146
void PvpMapHandler::spawnTreasureChests() {
	addTreasurePositions();
	const int32_t npcIds[] = {701388, 701388, 701388, 701388, 701388, 701389, 701389, 701389, 701390, 701390};
	for (int32_t id : npcIds) {
		spawnKeymasterOrTreasureChest(id, false);
	}
}

// Java PvpMapHandler.java:148-168
void PvpMapHandler::startRandomBossTask() {
	const services::cron::CronExpression* schedule = CustomConfig::PVP_MAP_RANDOM_BOSS_SCHEDULE.load();
	if (schedule == nullptr) // Java: CronService.schedule of a null expression
		throw runtime::NullPointerException("gameserver.pvpmap.randomboss.time");
	services::cron::CronService::getInstance().schedule(services::cron::CronJob({this}, [this] {
		if (!CustomConfig::PVP_MAP_ENABLED.load())
			return;
		int32_t bonus = static_cast<int32_t>(world::World::getInstance().getAllPlayers().size()) * 2;
		bonus = std::min(bonus, 30);
		if (commons::utils::Rnd::chance() < (CustomConfig::PVP_MAP_RANDOM_BOSS_BASE_RATE.load() + bonus)) {
			int32_t npcId = commons::utils::Rnd::get(RANDOM_BOSS_NPC_IDS);
			const model::templates::npc::NpcTemplate* npcTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
			runtime::Ref<model::templates::spawns::SpawnTemplate> spawnTemplate = spawnengine::SpawnEngine::newSingleTimeSpawn(mapId, npcId, 744.337f,
				292.986f, 233.697f, int8_t{43}, nullptr, "modified_iron_wall_aggressive");
			runtime::Ref<Npc> npc =
				model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), *spawnTemplate, npcTemplate);
			npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
			npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
			spawnengine::SpawnEngine::bringIntoWorld(*npc, mapId, instance->getInstanceId(), spawnTemplate->getX(), spawnTemplate->getY(),
				spawnTemplate->getZ(), spawnTemplate->getHeading());
			currentRandomBossObjId.set(npc->getObjectId());
			scheduleRandomBossDespawn();
			world::World::getInstance().forEachPlayer([](Player& player) { PvpMapService::getInstance().notifyBossSpawn(player); });
		}
	}), *schedule);
}

// Java PvpMapHandler.java:170-178
void PvpMapHandler::scheduleRandomBossDespawn() {
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<Npc> boss = runtime::as<Npc>(instance->getObject(currentRandomBossObjId.get()));
		if (boss != nullptr && !boss->getLifeStats()->isAboutToDie() && !boss->isDead()) {
			boss->getController().delete_();
			currentRandomBossObjId.set(0);
		}
	}, 50, utils::TimeUnit::MINUTES));
}

// Java PvpMapHandler.java:180-182
void PvpMapHandler::scheduleSupplyDespawn() {
	despawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({831980, 233192}); }, 120000));
}

// Java PvpMapHandler.java:184-188
void PvpMapHandler::join(Player& p) {
	if (canJoin(p)) {
		startTeleportation(p, false);
	}
}

// Java PvpMapHandler.java:190-196
void PvpMapHandler::leave(Player& p) {
	if (!checkState(p) || p.getController().hasScheduledTask(TaskId::SKILL_USE)) {
		PacketSendUtility::sendMessage(p, "You cannot leave the PvP-Map in your current state.");
		return;
	}
	startTeleportation(p, true);
}

// Java PvpMapHandler.java:198-221
void PvpMapHandler::startTeleportation(Player& p, bool isLeaving) {
	runtime::Ref<controllers::observer::ActionObserver> observer = getAllObserver(p);
	PacketSendUtility::broadcastPacket(p, SM_BIND_POINT_TELEPORT(1, p.getObjectId(), 1, 0), true);
	p.getObserveController()->attach(*observer);

	runtime::Ref<Player> player(p);
	p.getController().addTask(TaskId::SKILL_USE, ThreadPoolManager::getInstance().schedule({this}, [this, player, observer, isLeaving] {
		PacketSendUtility::broadcastPacket(*player, SM_BIND_POINT_TELEPORT(3, player->getObjectId(), 1, 0), true);
		ThreadPoolManager::getInstance().schedule({this}, [this, player, observer, isLeaving] {
			player->getObserveController()->removeObserver(*observer);
			player->getController().cancelTask(TaskId::SKILL_USE);
			if (!player->getController().isInCombat() && !player->getLifeStats()->isAboutToDie() && !player->isDead()) {
				if (isLeaving) {
					removePlayer(*player);
				} else {
					updateOrigin(*player);
					updateJoinOrLeaveTime(*player);
					instance->register_(player->getObjectId());
					runtime::Ref<WorldPosition> pos(*randomPosition(*respawnLocations.get(player->getRace())));
					services::teleport::TeleportService::teleportTo(*player, *instance, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading(),
						model::animations::TeleportAnimation::BATTLEGROUND);
				}
			}
		}, 1000);
	}, 10000));
}

// Java PvpMapHandler.java:223-231
runtime::Ref<controllers::observer::ActionObserver> PvpMapHandler::getAllObserver(Player& p) {
	return PvpMapHandler_ItemUseObserver::create(p);
}

// Java PvpMapHandler.java:233-258
bool PvpMapHandler::canJoin(Player& p) {
	if (!p.isStaff()) {
		if (!CustomConfig::PVP_MAP_ENABLED.load()) {
			PacketSendUtility::sendMessage(p, "The PvP-Map is currently disabled.");
			return false;
		} else if (p.getLevel() < 60) {
			PacketSendUtility::sendMessage(p, "The PvP-Map is for players level 60 and above.");
			return false;
		} else if (p.isInInstance() || p.getWorldId() == 400030000) {
			PacketSendUtility::sendMessage(p, "You cannot enter the PvP-Map while in an instance.");
			return false;
		} else if (p.getController().isInCombat()) {
			PacketSendUtility::sendMessage(p, "You cannot enter the PvP-Map while in combat.");
			return false;
		} else if (!checkState(p) || p.getController().hasScheduledTask(TaskId::SKILL_USE)) {
			PacketSendUtility::sendMessage(p, "You cannot enter the PvP-Map in your current state.");
			return false;
		} else if (joinOrLeaveTime.containsKey(p.getObjectId())
			&& ((commons::utils::currentTimeMillis() - *joinOrLeaveTime.get(p.getObjectId())) < 120000)) {
			int32_t timeInSeconds =
				static_cast<int32_t>(std::ceil(static_cast<float>(120000 - (commons::utils::currentTimeMillis() - *joinOrLeaveTime.get(p.getObjectId()))) / 1000.0f));
			PacketSendUtility::sendMessage(p, "You can reenter the PvP-Map in " + std::to_string(timeInSeconds) + " second" + (timeInSeconds > 1 ? "s." : "."));
			return false;
		}
	}
	return true;
}

// Java PvpMapHandler.java:260-265
bool PvpMapHandler::checkState(Player& p) {
	return !p.getController().isInCombat() && !p.getLifeStats()->isAboutToDie() && !p.isDead() && !p.isLooting() && !p.isInGlidingState()
		&& !p.isFlying() && !p.isUsingFlightTransporterOrWindstream() && !p.isInPlayerMode(model::actions::PlayerMode::RIDE)
		&& !p.hasStore() && p.getCastingSkill() == nullptr
		&& !p.getEffectController()->isInAnyAbnormalState(skillengine::effect::AbnormalState::CANT_ATTACK_STATE)
		&& !p.getEffectController()->isInAnyAbnormalState(skillengine::effect::AbnormalState::ROOT);
}

// Java PvpMapHandler.java:267-269 (synchronized)
void PvpMapHandler::updateOrigin(Player& p) {
	SYNCHRONIZED(*this) {
		origins.put(p.getObjectId(), runtime::Ref<WorldPosition>(*p.getPosition()));
	}
}

// Java PvpMapHandler.java:271-274 (synchronized)
void PvpMapHandler::updateJoinOrLeaveTime(Player& p) {
	SYNCHRONIZED(*this) {
		if (!p.isStaff())
			joinOrLeaveTime.put(p.getObjectId(), commons::utils::currentTimeMillis());
	}
}

// Java PvpMapHandler.java:276-288
bool PvpMapHandler::onReviveEvent(Player& player) {
	revive(player);
	if (!CustomConfig::PVP_MAP_ENABLED.load() || respawnLocations.isEmpty()) {
		if (instance->getPlayer(player.getObjectId()) != nullptr) {
			removePlayer(player);
		}
	} else {
		runtime::Ref<WorldPosition> pos(*randomPosition(*respawnLocations.get(player.getRace())));
		// C++-dev only (owner, 2026-10-24): the respawn moves the player at once and plays the battleground animations around it; Java's
		// teleportTo(..., BATTLEGROUND) waited for the client's animation before moving him (docs/deviations/P5-13.md)
		services::teleport::TeleportService::teleportInstantly(player, *instance, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading(),
			model::animations::TeleportAnimation::BATTLEGROUND);
	}
	return true;
}

// Java PvpMapHandler.java:290-300
bool PvpMapHandler::onDie(Player& player, model::gameobjects::Creature& lastAttacker) {
	if (CustomConfig::PVP_MAP_ENABLED.load()) {
		runtime::Ptr<Player> attacker = runtime::as<Player>(lastAttacker);
		if (attacker != nullptr && attacker.get() != &player) {
			spawnShugo(*attacker);
		}
		services::PvpService::getInstance().doReward(player, CustomConfig::PVP_MAP_AP_MULTIPLIER.load());
		announceDeath(player);
	}
	return true;
}

// Java PvpMapHandler.java:302-325
void PvpMapHandler::onDie(Npc& npc) {
	if (npc.getObjectId() == currentRandomBossObjId.get()) {
		currentRandomBossObjId.set(0);
		return;
	}
	auto positionOf = [this, &npc] {
		return world::WorldPosition::create(mapId, npc.getSpawn()->getX(), npc.getSpawn()->getY(), npc.getSpawn()->getZ(), npc.getSpawn()->getHeading());
	};
	switch (npc.getNpcId()) {
		case 219218: // keymaster chookuri
			keymasterPositions.add(positionOf());
			scheduleRespawn(npc.getNpcId(), commons::utils::Rnd::get(10, 15), true);
			break;
		case 219191: // keymaster zumita
		case 219192: // keymaster niksi
			keymasterPositions.add(positionOf());
			scheduleRespawn(npc.getNpcId(), commons::utils::Rnd::get(30, 50), true);
			break;
		case 219193: // keymaster dabra
			keymasterPositions.add(positionOf());
			scheduleRespawn(npc.getNpcId(), commons::utils::Rnd::get(110, 180), true);
			break;
		default:
			break;
	}
}

// Java PvpMapHandler.java:327-346
void PvpMapHandler::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	static_cast<void>(player);
	auto positionOf = [this, &npc] {
		return world::WorldPosition::create(mapId, npc.getSpawn()->getX(), npc.getSpawn()->getY(), npc.getSpawn()->getZ(), npc.getSpawn()->getHeading());
	};
	switch (npc.getNpcId()) {
		case 701388:
			treasurePositions.add(positionOf());
			scheduleRespawn(npc.getNpcId(), commons::utils::Rnd::get(10, 20), false);
			break;
		case 701389:
			treasurePositions.add(positionOf());
			scheduleRespawn(npc.getNpcId(), commons::utils::Rnd::get(30, 60), false);
			break;
		case 701390:
			treasurePositions.add(positionOf());
			scheduleRespawn(npc.getNpcId(), commons::utils::Rnd::get(120, 200), false);
			break;
		default:
			break;
	}
}

// Java PvpMapHandler.java:348-356
void PvpMapHandler::announceDeath(Player& player) {
	if (!player.isStaff() && player.getAbyssRank() != nullptr) {
		std::optional<std::string> zoneNameL10n = getZoneNameL10n(player);
		if (zoneNameL10n)
			PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(player, *zoneNameL10n));
		else
			PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(player));
	}
}

// Java PvpMapHandler.java:358-369
void PvpMapHandler::onEnterInstance(Player& player) {
	if (!player.isStaff()) {
		updateJoinOrLeaveTime(player);
		instance->forEachPlayer([&player](Player& p) {
			if (&p != &player)
				PacketSendUtility::sendMessage(p, "A new player has joined!", ChatType::BRIGHT_YELLOW_CENTER);
		});
		const model::Race race = player.getRace();
		PacketSendUtility::broadcastToWorld(SM_MESSAGE(0, "", "An enemy has entered the PvP-Map!", ChatType::BRIGHT_YELLOW_CENTER),
			[race](Player& p) { return p.getLevel() >= 60 && !p.isInInstance() && p.getRace() != race; });
	}
}

// Java PvpMapHandler.java:371-375
void PvpMapHandler::onLeaveInstance(Player& player) {
	GeneralInstanceHandler::onLeaveInstance(player);
	updateJoinOrLeaveTime(player);
}

// Java PvpMapHandler.java:377-380
void PvpMapHandler::onPlayerLogout(Player& player) {
	removePlayer(player);
}

// Java PvpMapHandler.java:382-386
void PvpMapHandler::onInstanceDestroy() {
	PvpMapService::getInstance().onInstanceDestroy();
	cancelTasks();
}

// Java PvpMapHandler.java:388-396
void PvpMapHandler::cancelTasks() {
	if (runtime::Ptr<runtime::Future> task = supplyTask.get(); task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
	if (runtime::Ptr<runtime::Future> task = despawnTask.get(); task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
	for (runtime::Ptr<runtime::Future> task : tasks)
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
}

// Java PvpMapHandler.java:398-417
bool PvpMapHandler::spawnAllowed() {
	if (!CustomConfig::PVP_MAP_ENABLED.load())
		return false;
	int8_t asmodians = 0;
	int8_t elyos = 0;
	for (runtime::Ptr<Player> player : instance->getPlayersInside()) {
		if (player->isStaff()) {
			continue;
		} else if (player->getRace() == model::Race::ASMODIANS) {
			asmodians++;
		} else {
			elyos++;
		}
		if (asmodians > 1 && elyos > 1) {
			return true;
		}
	}
	return false;
}

// Java PvpMapHandler.java:419-427
int32_t PvpMapHandler::getParticipantsSize() {
	int32_t playerCount = 0;
	for (runtime::Ptr<Player> p : instance->getPlayersInside()) {
		if (!p->isStaff()) {
			playerCount++;
		}
	}
	return playerCount;
}

// Java PvpMapHandler.java:429-439 (synchronized)
void PvpMapHandler::removePlayer(Player& p) {
	SYNCHRONIZED(*this) {
		updateJoinOrLeaveTime(p);
		if (p.isDead())
			revive(p);
		runtime::Ptr<WorldPosition> position = origins.remove(p.getObjectId());
		if (position != nullptr && !isAtVulnerableFortress(position->getMapId(), position->getX(), position->getY(), position->getZ())) {
			services::teleport::TeleportService::teleportTo(p, *position);
		} else {
			services::teleport::TeleportService::moveToBindLocation(p);
		}
	}
}

// Java PvpMapHandler.java:441-446
void PvpMapHandler::revive(Player& player) {
	services::player::PlayerReviveService::revive(player, 100, 100, false, 0);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	player.getGameStats()->updateStatsAndSpeedVisually();
	player.unsetResPosState();
}

// Java PvpMapHandler.java:448-451
bool PvpMapHandler::isAtVulnerableFortress(int32_t worldId, float x, float y, float z) {
	runtime::Ptr<model::siege::FortressLocation> fortress = services::SiegeService::getInstance().findFortress(worldId, x, y, z);
	return fortress != nullptr && fortress->isVulnerable();
}

// Java PvpMapHandler.java:453-455
bool PvpMapHandler::isOnMap(model::gameobjects::Creature& creature) {
	return instance != nullptr && instance->getObject(creature.getObjectId()) != nullptr;
}

// Java PvpMapHandler.java:457-459
bool PvpMapHandler::isRandomBoss(int32_t objectId) {
	return currentRandomBossObjId.get() == objectId;
}

// Java PvpMapHandler.java:461-464
bool PvpMapHandler::isRandomBossAlive() {
	runtime::Ptr<Npc> boss = runtime::as<Npc>(instance->getObject(currentRandomBossObjId.get()));
	return boss != nullptr && !boss->isDead();
}

// Java PvpMapHandler.java:824-832
std::optional<std::string> PvpMapHandler::getZoneNameL10n(Player& player) {
	for (runtime::Ptr<world::zone::ZoneInstance> zone : player.findZones()) {
		int32_t zoneNameL10nId = getZoneNameL10nId(zone->getAreaTemplate()->getZoneName()->name());
		if (zoneNameL10nId > 0) {
			return utils::ChatUtil::l10n(zoneNameL10nId);
		}
	}
	return std::nullopt;
}

// Java PvpMapHandler.java:854-857
float PvpMapHandler::getApMultiplier() {
	return CustomConfig::PVP_MAP_PVE_AP_MULTIPLIER.load();
}

// Java PvpMapHandler.java: spawnNpcs
void PvpMapHandler::spawnNpcs() {
	spawnAndSetRespawn(218544, 739.08954f, 743.7691f, 194.63808f, int8_t{69}, 295);
	spawnAndSetRespawn(218544, 790.8744f, 522.2378f, 228.78705f, int8_t{90}, 295);
	spawnAndSetRespawn(218544, 823.72784f, 548.57434f, 235.62047f, int8_t{77}, 295);
	spawnAndSetRespawn(218544, 793.6772f, 596.01904f, 240.03558f, int8_t{8}, 295);
	spawnAndSetRespawn(218544, 795.2162f, 584.24866f, 239.5659f, int8_t{0}, 295);
	spawnAndSetRespawn(218544, 357.8682f, 479.70685f, 237.4225f, int8_t{99}, 295);
	spawnAndSetRespawn(218544, 367.3182f, 485.55405f, 237.4225f, int8_t{108}, 295);
	spawnAndSetRespawn(218544, 353.6212f, 427.49295f, 233.09781f, int8_t{96}, 295);
	spawnAndSetRespawn(218544, 757.8844f, 748.44446f, 195.72215f, int8_t{71}, 295);
	spawnAndSetRespawn(218544, 752.23975f, 757.58105f, 195.71138f, int8_t{69}, 295);
	spawnAndSetRespawn(218544, 430.81192f, 758.72595f, 203.42834f, int8_t{14}, 295);
	spawnAndSetRespawn(218544, 433.25076f, 779.33185f, 203.42834f, int8_t{108}, 295);
	spawnAndSetRespawn(218544, 471.17935f, 762.66144f, 201.68672f, int8_t{3}, 295);
	spawnAndSetRespawn(218544, 769.0259f, 781.72473f, 198.76245f, int8_t{79}, 295);
	spawnAndSetRespawn(218544, 772.4939f, 762.43164f, 198.0455f, int8_t{70}, 295);
	spawnAndSetRespawn(218544, 568.91833f, 508.94455f, 217.75f, int8_t{118}, 295);
	spawnAndSetRespawn(218544, 595.42993f, 504.22293f, 217.66063f, int8_t{69}, 295);
	spawnAndSetRespawn(218544, 656.7253f, 219.83476f, 238.48415f, int8_t{46}, 295);
	spawnAndSetRespawn(218544, 653.7101f, 370.04272f, 239.61528f, int8_t{103}, 295);
	spawnAndSetRespawn(218544, 762.97675f, 386.17636f, 242.0815f, int8_t{23}, 295);
	spawnAndSetRespawn(218544, 826.2748f, 351.18307f, 243.75453f, int8_t{46}, 295);
	spawnAndSetRespawn(218544, 781.19916f, 330.00568f, 253.43387f, int8_t{76}, 295);
	spawnAndSetRespawn(218544, 698.00366f, 262.089f, 253.43388f, int8_t{15}, 295);
	spawnAndSetRespawn(218544, 644.2379f, 408.7981f, 242.47498f, int8_t{103}, 295);
	spawnAndSetRespawn(218544, 644.08527f, 290.91754f, 225.69778f, int8_t{9}, 295);
	spawnAndSetRespawn(218544, 549.149f, 424.63623f, 222.66476f, int8_t{18}, 295);
	spawnAndSetRespawn(218544, 655.04407f, 242.13554f, 225.69778f, int8_t{22}, 295);
	spawnAndSetRespawn(218544, 457.68298f, 276.2121f, 246.71693f, int8_t{75}, 295);
	spawnAndSetRespawn(218544, 684.6678f, 308.9507f, 225.69778f, int8_t{61}, 295);
	spawnAndSetRespawn(218544, 449.93518f, 284.11102f, 245.73611f, int8_t{26}, 295);
	spawnAndSetRespawn(218544, 401.42535f, 259.03088f, 253.28592f, int8_t{20}, 295);
	spawnAndSetRespawn(218544, 423.5366f, 269.79514f, 247.5003f, int8_t{97}, 295);
	spawnAndSetRespawn(218544, 798.90045f, 366.7033f, 230.98207f, int8_t{80}, 295);
	spawnAndSetRespawn(218544, 438.80832f, 618.10834f, 214.52452f, int8_t{42}, 295);
	spawnAndSetRespawn(218544, 429.44916f, 614.982f, 214.52452f, int8_t{40}, 295);
	spawnAndSetRespawn(218544, 422.44595f, 647.92f, 214.52452f, int8_t{95}, 295);
	spawnAndSetRespawn(218544, 576.42346f, 716.5756f, 205.78198f, int8_t{54}, 295);
	spawnAndSetRespawn(218544, 317.8458f, 684.38007f, 212.99036f, int8_t{67}, 295);
	spawnAndSetRespawn(218544, 309.91922f, 682.13055f, 212.75505f, int8_t{6}, 295);
	spawnAndSetRespawn(218544, 239.23927f, 757.8234f, 201.60623f, int8_t{113}, 295);
	spawnAndSetRespawn(218544, 252.59685f, 769.42523f, 201.95093f, int8_t{93}, 295);
	spawnAndSetRespawn(218544, 546.30115f, 686.9413f, 205.5f, int8_t{46}, 295);
	spawnAndSetRespawn(218544, 603.9977f, 876.9201f, 192.91196f, int8_t{89}, 295);
	spawnAndSetRespawn(218544, 603.13055f, 864.87823f, 192.65533f, int8_t{89}, 295);
	spawnAndSetRespawn(218544, 690.58f, 759.3537f, 182.375f, int8_t{55}, 295);
	spawnAndSetRespawn(218544, 685.9591f, 746.5975f, 182.375f, int8_t{55}, 295);
	spawnAndSetRespawn(218544, 721.9566f, 723.7783f, 189.375f, int8_t{67}, 295);
	spawnAndSetRespawn(218544, 694.7766f, 730.0601f, 188.96326f, int8_t{0}, 295);
	spawnAndSetRespawn(218544, 714.0073f, 739.8774f, 189.24994f, int8_t{75}, 295);
	spawnAndSetRespawn(218544, 717.8495f, 731.7268f, 189.3656f, int8_t{70}, 295);
	spawnAndSetRespawn(218547, 757.2226f, 709.5766f, 194.62617f, int8_t{43}, 295);
	spawnAndSetRespawn(218547, 815.31647f, 537.01685f, 229.99895f, int8_t{70}, 295);
	spawnAndSetRespawn(218547, 808.0359f, 568.60266f, 239.5f, int8_t{25}, 295);
	spawnAndSetRespawn(218547, 813.6234f, 583.6792f, 239.39505f, int8_t{115}, 295);
	spawnAndSetRespawn(218547, 340.38742f, 444.74323f, 234.125f, int8_t{10}, 295);
	spawnAndSetRespawn(218547, 455.97104f, 778.36456f, 202.01328f, int8_t{96}, 295);
	spawnAndSetRespawn(218547, 517.13416f, 764.3607f, 195.46646f, int8_t{118}, 295);
	spawnAndSetRespawn(218547, 727.2865f, 779.1507f, 194.5f, int8_t{108}, 295);
	spawnAndSetRespawn(218547, 607.4919f, 490.93564f, 217.90976f, int8_t{46}, 295);
	spawnAndSetRespawn(218547, 607.90454f, 508.69162f, 218.125f, int8_t{68}, 295);
	spawnAndSetRespawn(218547, 607.9011f, 520.136f, 217.875f, int8_t{14}, 295);
	spawnAndSetRespawn(218547, 597.78455f, 480.55164f, 218.0f, int8_t{98}, 295);
	spawnAndSetRespawn(218547, 633.9897f, 228.40494f, 238.07529f, int8_t{22}, 295);
	spawnAndSetRespawn(218547, 620.4969f, 311.71567f, 236.74612f, int8_t{114}, 295);
	spawnAndSetRespawn(218547, 640.37787f, 304.753f, 236.92535f, int8_t{54}, 295);
	spawnAndSetRespawn(218547, 740.5545f, 388.09833f, 242.1149f, int8_t{33}, 295);
	spawnAndSetRespawn(218547, 796.8819f, 396.5597f, 242.01622f, int8_t{83}, 295);
	spawnAndSetRespawn(218547, 748.0393f, 322.6207f, 249.28568f, int8_t{43}, 295);
	spawnAndSetRespawn(218547, 714.6477f, 285.88177f, 249.28185f, int8_t{42}, 295);
	spawnAndSetRespawn(218547, 547.2966f, 433.86728f, 222.75f, int8_t{112}, 295);
	spawnAndSetRespawn(218547, 638.9781f, 426.24887f, 242.47498f, int8_t{90}, 295);
	spawnAndSetRespawn(218547, 673.12494f, 221.4442f, 225.69778f, int8_t{33}, 295);
	spawnAndSetRespawn(218547, 461.76224f, 266.64352f, 246.5f, int8_t{60}, 295);
	spawnAndSetRespawn(218547, 737.5307f, 352.08997f, 230.94298f, int8_t{43}, 295);
	spawnAndSetRespawn(218547, 396.4398f, 282.85092f, 253.6672f, int8_t{85}, 295);
	spawnAndSetRespawn(218547, 445.27808f, 268.4044f, 246.47473f, int8_t{79}, 295);
	spawnAndSetRespawn(218547, 241.67007f, 743.7223f, 201.54707f, int8_t{22}, 295);
	spawnAndSetRespawn(218547, 274.27524f, 735.2498f, 205.57104f, int8_t{22}, 295);
	spawnAndSetRespawn(218547, 535.81146f, 693.7665f, 205.38959f, int8_t{3}, 295);
	spawnAndSetRespawn(218547, 594.31903f, 854.5406f, 192.18222f, int8_t{101}, 295);
	spawnAndSetRespawn(218547, 696.12354f, 705.2198f, 194.81421f, int8_t{8}, 295);
	spawnAndSetRespawn(219189, 671.8091f, 737.7017f, 178.73135f, int8_t{43}, 295);
	spawnAndSetRespawn(219189, 593.70734f, 547.63635f, 219.09225f, int8_t{7}, 295);
	spawnAndSetRespawn(219189, 565.2322f, 812.2663f, 188.3649f, int8_t{105}, 295);
	spawnAndSetRespawn(219189, 622.64264f, 825.0713f, 188.46423f, int8_t{69}, 295);
	spawnAndSetRespawn(219166, 588.5865f, 546.58716f, 219.3217f, int8_t{68}, 295);
	spawnAndSetRespawn(219190, 808.65955f, 464.9168f, 228.91623f, int8_t{48}, 295);
	spawnAndSetRespawn(219190, 648.5453f, 381.37613f, 228.625f, int8_t{60}, 295);
	spawnAndSetRespawn(219190, 669.57f, 402.74194f, 228.61024f, int8_t{32}, 295);
	spawnAndSetRespawn(219190, 476.58044f, 681.6464f, 217.96188f, int8_t{38}, 295);
	spawnAndSetRespawn(219195, 810.7329f, 453.15427f, 228.75f, int8_t{81}, 295);
	spawnAndSetRespawn(219195, 725.1208f, 547.77325f, 233.37512f, int8_t{70}, 295);
	spawnAndSetRespawn(219195, 458.33813f, 530.78235f, 222.37468f, int8_t{49}, 295);
	spawnAndSetRespawn(219195, 706.0179f, 490.12515f, 234.89288f, int8_t{100}, 295);
	spawnAndSetRespawn(219195, 346.1327f, 655.7598f, 219.9216f, int8_t{52}, 295);
	spawnAndSetRespawn(219195, 669.57355f, 774.0344f, 181.60016f, int8_t{93}, 295);
	spawnAndSetRespawn(218545, 808.67224f, 525.32306f, 230.087f, int8_t{67}, 295);
	spawnAndSetRespawn(218545, 825.1957f, 561.02435f, 239.06339f, int8_t{82}, 295);
	spawnAndSetRespawn(218545, 799.7665f, 572.0052f, 239.54282f, int8_t{20}, 295);
	spawnAndSetRespawn(218545, 809.45435f, 597.70325f, 239.5659f, int8_t{88}, 295);
	spawnAndSetRespawn(218545, 820.66394f, 592.6436f, 239.41522f, int8_t{79}, 295);
	spawnAndSetRespawn(218545, 796.40765f, 606.99194f, 239.88962f, int8_t{103}, 295);
	spawnAndSetRespawn(218545, 662.76245f, 547.9632f, 238.60799f, int8_t{93}, 295);
	spawnAndSetRespawn(218545, 372.4097f, 442.73737f, 234.19669f, int8_t{65}, 295);
	spawnAndSetRespawn(218545, 348.93497f, 491.24512f, 239.26987f, int8_t{107}, 295);
	spawnAndSetRespawn(218545, 732.9599f, 754.0701f, 194.79166f, int8_t{100}, 295);
	spawnAndSetRespawn(218545, 403.51105f, 491.78015f, 233.7788f, int8_t{12}, 295);
	spawnAndSetRespawn(218545, 745.4872f, 733.6486f, 194.79395f, int8_t{40}, 295);
	spawnAndSetRespawn(218545, 447.26068f, 752.36f, 202.51537f, int8_t{21}, 295);
	spawnAndSetRespawn(218545, 481.4032f, 769.2115f, 201.59654f, int8_t{113}, 295);
	spawnAndSetRespawn(218545, 771.4963f, 720.42f, 194.5f, int8_t{53}, 295);
	spawnAndSetRespawn(218545, 776.93744f, 742.47125f, 195.54028f, int8_t{80}, 295);
	spawnAndSetRespawn(218545, 755.26324f, 776.8493f, 195.54028f, int8_t{86}, 295);
	spawnAndSetRespawn(218545, 691.7244f, 638.8437f, 203.85924f, int8_t{33}, 295);
	spawnAndSetRespawn(218545, 710.84985f, 672.1933f, 206.10843f, int8_t{49}, 295);
	spawnAndSetRespawn(218545, 609.9847f, 498.69614f, 218.12404f, int8_t{61}, 295);
	spawnAndSetRespawn(218545, 596.21906f, 522.6787f, 217.75f, int8_t{81}, 295);
	spawnAndSetRespawn(218545, 621.87305f, 248.52107f, 236.5246f, int8_t{3}, 295);
	spawnAndSetRespawn(218545, 643.08826f, 252.42151f, 236.9016f, int8_t{64}, 295);
	spawnAndSetRespawn(218545, 737.86536f, 408.9785f, 242.11967f, int8_t{93}, 295);
	spawnAndSetRespawn(218545, 789.3389f, 376.88342f, 242.2093f, int8_t{23}, 295);
	spawnAndSetRespawn(218545, 788.5563f, 309.24756f, 253.43407f, int8_t{53}, 295);
	spawnAndSetRespawn(218545, 713.9743f, 317.5257f, 252.13892f, int8_t{16}, 295);
	spawnAndSetRespawn(218545, 721.78467f, 324.17868f, 252.13892f, int8_t{73}, 295);
	spawnAndSetRespawn(218545, 724.86456f, 248.67912f, 253.43427f, int8_t{34}, 295);
	spawnAndSetRespawn(218545, 566.43604f, 414.4239f, 222.67874f, int8_t{35}, 295);
	spawnAndSetRespawn(218545, 788.57983f, 572.7258f, 239.375f, int8_t{104}, 295);
	spawnAndSetRespawn(218545, 682.3254f, 272.88013f, 225.69778f, int8_t{49}, 295);
	spawnAndSetRespawn(218545, 558.0802f, 435.9432f, 222.75f, int8_t{98}, 295);
	spawnAndSetRespawn(218545, 692.28894f, 351.46298f, 228.7785f, int8_t{103}, 295);
	spawnAndSetRespawn(218545, 455.10605f, 258.98337f, 246.5f, int8_t{43}, 295);
	spawnAndSetRespawn(218545, 385.7541f, 270.29462f, 253.5f, int8_t{7}, 295);
	spawnAndSetRespawn(218545, 815.5708f, 350.05713f, 230.98207f, int8_t{73}, 295);
	spawnAndSetRespawn(218545, 446.28192f, 270.5887f, 246.46321f, int8_t{32}, 295);
	spawnAndSetRespawn(218545, 313.05374f, 623.97473f, 230.95587f, int8_t{47}, 295);
	spawnAndSetRespawn(218545, 283.17975f, 754.6061f, 203.93437f, int8_t{79}, 295);
	spawnAndSetRespawn(218545, 533.2114f, 704.00757f, 205.625f, int8_t{7}, 295);
	spawnAndSetRespawn(218545, 582.3275f, 696.94165f, 210.41594f, int8_t{72}, 295);
	spawnAndSetRespawn(218545, 619.1518f, 878.12915f, 193.24794f, int8_t{69}, 295);
	spawnAndSetRespawn(218545, 697.3136f, 750.4479f, 185.59483f, int8_t{53}, 295);
	spawnAndSetRespawn(218545, 726.8993f, 742.47095f, 193.55424f, int8_t{69}, 295);
	spawnAndSetRespawn(218545, 731.63965f, 733.70996f, 193.5039f, int8_t{69}, 295);
	spawnAndSetRespawn(218549, 795.813f, 541.2819f, 229.90819f, int8_t{100}, 295);
	spawnAndSetRespawn(218549, 795.4988f, 557.5386f, 240.98943f, int8_t{11}, 295);
	spawnAndSetRespawn(218549, 808.0439f, 611.9442f, 239.5659f, int8_t{81}, 295);
	spawnAndSetRespawn(218549, 833.02f, 599.4704f, 239.46935f, int8_t{86}, 295);
	spawnAndSetRespawn(218549, 381.9712f, 458.69794f, 235.71736f, int8_t{49}, 295);
	spawnAndSetRespawn(218549, 445.28845f, 785.9884f, 202.92975f, int8_t{84}, 295);
	spawnAndSetRespawn(218549, 737.7692f, 793.7346f, 195.6785f, int8_t{85}, 295);
	spawnAndSetRespawn(218549, 450.3595f, 767.3717f, 202.3945f, int8_t{107}, 295);
	spawnAndSetRespawn(218549, 497.7351f, 762.2599f, 200.00084f, int8_t{11}, 295);
	spawnAndSetRespawn(218549, 805.90955f, 737.58636f, 196.34206f, int8_t{66}, 295);
	spawnAndSetRespawn(218549, 572.9683f, 490.3728f, 217.75f, int8_t{113}, 295);
	spawnAndSetRespawn(218549, 584.0002f, 499.08987f, 217.75002f, int8_t{107}, 295);
	spawnAndSetRespawn(218549, 630.12134f, 338.17273f, 236.74385f, int8_t{111}, 295);
	spawnAndSetRespawn(218549, 710.1343f, 405.6931f, 241.98926f, int8_t{92}, 295);
	spawnAndSetRespawn(218549, 809.4866f, 361.95856f, 241.92001f, int8_t{19}, 295);
	spawnAndSetRespawn(218549, 777.9167f, 287.46475f, 253.43427f, int8_t{65}, 295);
	spawnAndSetRespawn(218549, 576.3781f, 400.8725f, 222.4382f, int8_t{116}, 295);
	spawnAndSetRespawn(218549, 741.08673f, 257.78488f, 253.4343f, int8_t{27}, 295);
	spawnAndSetRespawn(218549, 685.96643f, 344.79675f, 245.26868f, int8_t{14}, 295);
	spawnAndSetRespawn(218549, 701.34973f, 357.53714f, 245.2944f, int8_t{73}, 295);
	spawnAndSetRespawn(218549, 692.49194f, 243.17621f, 227.16022f, int8_t{61}, 295);
	spawnAndSetRespawn(218549, 643.6444f, 268.54294f, 225.69778f, int8_t{5}, 295);
	spawnAndSetRespawn(218549, 560.1584f, 423.3512f, 222.625f, int8_t{17}, 295);
	spawnAndSetRespawn(218549, 435.71008f, 255.26958f, 246.375f, int8_t{24}, 295);
	spawnAndSetRespawn(218549, 702.438f, 373.9413f, 228.674f, int8_t{115}, 295);
	spawnAndSetRespawn(218549, 805.08105f, 323.8884f, 230.98207f, int8_t{32}, 295);
	spawnAndSetRespawn(218549, 556.52484f, 707.8591f, 206.58656f, int8_t{41}, 295);
	spawnAndSetRespawn(218549, 566.68835f, 675.889f, 211.4778f, int8_t{33}, 295);
	spawnAndSetRespawn(218549, 595.42f, 832.61285f, 188.6633f, int8_t{93}, 295);
	spawnAndSetRespawn(218549, 610.9518f, 854.17535f, 192.2f, int8_t{75}, 295);
	spawnAndSetRespawn(218549, 605.73267f, 832.3487f, 188.59225f, int8_t{94}, 295);
	spawnAndSetRespawn(218549, 706.22314f, 756.8924f, 188.98705f, int8_t{76}, 295);
	spawnAndSetRespawn(218549, 733.72406f, 719.50574f, 194.5f, int8_t{25}, 295);
	spawnAndSetRespawn(218549, 717.5115f, 753.83606f, 194.45918f, int8_t{108}, 295);
	spawnAndSetRespawn(218546, 807.58453f, 549.3488f, 238.99246f, int8_t{24}, 295);
	spawnAndSetRespawn(218546, 831.0175f, 575.88043f, 239.375f, int8_t{55}, 295);
	spawnAndSetRespawn(218546, 834.5515f, 587.1165f, 239.47925f, int8_t{66}, 295);
	spawnAndSetRespawn(218546, 346.86578f, 460.7019f, 235.10414f, int8_t{106}, 295);
	spawnAndSetRespawn(218546, 440.78027f, 531.0118f, 223.15675f, int8_t{0}, 295);
	spawnAndSetRespawn(218546, 430.2271f, 769.12067f, 204.05684f, int8_t{119}, 295);
	spawnAndSetRespawn(218546, 796.9759f, 722.1847f, 196.7664f, int8_t{31}, 295);
	spawnAndSetRespawn(218546, 736.8544f, 768.3472f, 194.5534f, int8_t{87}, 295);
	spawnAndSetRespawn(218546, 581.5761f, 481.5813f, 217.75f, int8_t{33}, 295);
	spawnAndSetRespawn(218546, 581.79236f, 515.9131f, 217.75f, int8_t{98}, 295);
	spawnAndSetRespawn(218546, 617.6701f, 275.5021f, 236.78897f, int8_t{4}, 295);
	spawnAndSetRespawn(218546, 639.31323f, 280.07794f, 236.37653f, int8_t{63}, 295);
	spawnAndSetRespawn(218546, 681.21075f, 391.91763f, 240.07115f, int8_t{104}, 295);
	spawnAndSetRespawn(218546, 770.5945f, 406.5085f, 241.92291f, int8_t{82}, 295);
	spawnAndSetRespawn(218546, 734.4841f, 343.54346f, 249.30322f, int8_t{87}, 295);
	spawnAndSetRespawn(218546, 694.2494f, 308.95227f, 249.30322f, int8_t{1}, 295);
	spawnAndSetRespawn(218546, 569.8869f, 435.66293f, 222.04213f, int8_t{17}, 295);
	spawnAndSetRespawn(218546, 684.5185f, 260.11212f, 225.69778f, int8_t{76}, 295);
	spawnAndSetRespawn(218546, 706.58984f, 334.94543f, 229.35587f, int8_t{44}, 295);
	spawnAndSetRespawn(218546, 437.9635f, 277.951f, 246.375f, int8_t{105}, 295);
	spawnAndSetRespawn(218546, 299.42566f, 722.23926f, 205.89642f, int8_t{74}, 295);
	spawnAndSetRespawn(218546, 557.1041f, 691.66016f, 205.91748f, int8_t{27}, 295);
	spawnAndSetRespawn(218546, 588.75586f, 871.6779f, 192.84047f, int8_t{110}, 295);
	spawnAndSetRespawn(218546, 694.68243f, 715.9783f, 193.79282f, int8_t{1}, 295);
	spawnAndSetRespawn(218546, 742.9393f, 709.2019f, 194.5f, int8_t{22}, 295);
	spawnAndSetRespawn(218920, 670.0766f, 560.6434f, 229.34996f, int8_t{97}, 295);
	spawnAndSetRespawn(218920, 685.3019f, 427.8387f, 229.82187f, int8_t{2}, 295);
	spawnAndSetRespawn(218920, 757.7039f, 356.35028f, 232.42679f, int8_t{14}, 295);
	spawnAndSetRespawn(218920, 618.3982f, 361.9023f, 224.94342f, int8_t{65}, 295);
	spawnAndSetRespawn(218920, 517.9982f, 230.86314f, 231.92047f, int8_t{117}, 295);
	spawnAndSetRespawn(218548, 467.81332f, 551.91583f, 216.59146f, int8_t{47}, 295);
	spawnAndSetRespawn(218548, 489.4462f, 736.5246f, 209.70947f, int8_t{4}, 295);
	spawnAndSetRespawn(218548, 266.49475f, 605.3711f, 223.27095f, int8_t{7}, 295);
	spawnAndSetRespawn(218548, 576.5212f, 229.43967f, 232.6177f, int8_t{19}, 295);
	spawnAndSetRespawn(218548, 703.4563f, 466.252f, 227.25f, int8_t{69}, 295);
	spawnAndSetRespawn(218548, 520.92334f, 475.87265f, 216.5186f, int8_t{101}, 295);
	spawnAndSetRespawn(218548, 309.666f, 650.7151f, 214.44623f, int8_t{31}, 295);
	spawnAndSetRespawn(218548, 483.80945f, 689.0492f, 216.82463f, int8_t{39}, 295);
	spawnAndSetRespawn(218548, 530.01953f, 721.6219f, 203.44101f, int8_t{38}, 295);
	spawnAndSetRespawn(218548, 584.07904f, 794.46204f, 187.90817f, int8_t{61}, 295);
	spawnAndSetRespawn(218548, 634.5419f, 739.6843f, 183.52533f, int8_t{19}, 295);
	spawnAndSetRespawn(219167, 306.2718f, 449.41235f, 234.73024f, int8_t{58}, 295);
	spawnAndSetRespawn(219167, 406.56827f, 561.6233f, 214.85794f, int8_t{24}, 295);
	spawnAndSetRespawn(219167, 470.27045f, 382.566f, 241.05424f, int8_t{32}, 295);
	spawnAndSetRespawn(219167, 570.3662f, 364.65964f, 226.03668f, int8_t{3}, 295);
	spawnAndSetRespawn(219167, 491.91467f, 675.4431f, 220.99588f, int8_t{43}, 295);
	spawnAndSetRespawn(219167, 477.62268f, 668.1604f, 220.99417f, int8_t{30}, 295);
	spawnAndSetRespawn(219167, 604.9417f, 803.43274f, 186.88289f, int8_t{12}, 295);
	spawnAndSetRespawn(219196, 417.46832f, 312.88907f, 233.34177f, int8_t{96}, 295);
	spawnAndSetRespawn(219198, 378.59048f, 365.98883f, 226.03691f, int8_t{54}, 295);
	spawnAndSetRespawn(219198, 487.1122f, 663.8798f, 221.72038f, int8_t{38}, 295);
	spawnAndSetRespawn(219169, 642.6832f, 562.6061f, 229.95897f, int8_t{95}, 295);
	spawnAndSetRespawn(219169, 616.85236f, 237.28638f, 229.79572f, int8_t{56}, 295);
	spawnAndSetRespawn(219169, 629.52075f, 426.4336f, 226.93431f, int8_t{42}, 295);
	spawnAndSetRespawn(219169, 484.21207f, 411.73413f, 233.46696f, int8_t{10}, 295);
	spawnAndSetRespawn(219188, 804.1674f, 406.99316f, 232.52094f, int8_t{55}, 295);
	spawnAndSetRespawn(219188, 676.3855f, 489.59888f, 226.12563f, int8_t{94}, 295);
	spawnAndSetRespawn(219188, 299.4595f, 699.49084f, 207.92665f, int8_t{20}, 295);
	spawnAndSetRespawn(219188, 259.73505f, 750.9975f, 201.32014f, int8_t{23}, 295);
	spawnAndSetRespawn(219188, 266.61508f, 748.002f, 201.34828f, int8_t{24}, 295);
	spawnAndSetRespawn(219188, 595.1314f, 781.1905f, 186.65916f, int8_t{89}, 295);
	spawnAndSetRespawn(218551, 737.00867f, 431.07535f, 230.37608f, int8_t{69}, 295);
	spawnAndSetRespawn(218551, 359.74063f, 661.96454f, 217.18471f, int8_t{23}, 295);
	spawnAndSetRespawn(219194, 495.03677f, 439.18695f, 223.67601f, int8_t{108}, 295);
	spawnAndSetRespawn(219194, 615.3467f, 265.0791f, 226.51422f, int8_t{65}, 295);
	spawnAndSetRespawn(219194, 435.5179f, 681.1302f, 214.8404f, int8_t{109}, 295);
	spawnAndSetRespawn(219194, 519.0116f, 780.06616f, 194.38846f, int8_t{104}, 295);
	spawnAndSetRespawn(218550, 499.67398f, 422.29724f, 226.32646f, int8_t{53}, 295);
	spawnAndSetRespawn(218550, 701.68555f, 410.91293f, 231.0f, int8_t{43}, 295);
	spawnAndSetRespawn(218550, 647.8726f, 481.46146f, 226.34134f, int8_t{103}, 295);
	spawnAndSetRespawn(218550, 451.62155f, 714.1448f, 213.3955f, int8_t{5}, 295);
	spawnAndSetRespawn(219187, 581.25275f, 333.01978f, 227.84341f, int8_t{7}, 295);
	spawnAndSetRespawn(219187, 438.07648f, 695.8468f, 215.41328f, int8_t{109}, 295);
	spawnAndSetRespawn(219197, 591.01575f, 812.62335f, 186.81348f, int8_t{37}, 295);

	// guardian tower
	spawnAndSetRespawn(233509, 704.6f, 636.3f, 212.239f, int8_t{37}, 295); // asmodian
	spawnAndSetRespawn(233509, 712.0f, 639.2f, 212.271f, int8_t{37}, 295); // asmodian
	spawnAndSetRespawn(233509, 589.2f, 699.7f, 220.973f, int8_t{73}, 295); // asmodian
	spawnAndSetRespawn(233509, 583.7f, 706.6f, 221.170f, int8_t{73}, 295); // asmodian

	spawnAndSetRespawn(233529, 288.5f, 391.0f, 238.445f, int8_t{14}, 295); // elyos
	spawnAndSetRespawn(233529, 282.9f, 397.2f, 238.200f, int8_t{14}, 295); // elyos
	spawnAndSetRespawn(233529, 330.5f, 626.5f, 247.564f, int8_t{45}, 295); // elyos
	spawnAndSetRespawn(233529, 336.0f, 632.0f, 247.601f, int8_t{45}, 295); // elyos

}

// Java PvpMapHandler.java: addRespawnLocations
void PvpMapHandler::addRespawnLocations() {
	respawnLocations.clear();
	respawnLocations.put(model::Race::ELYOS, runtime::RcArrayList<runtime::Ref<world::WorldPosition>>::create(AION_LOCK_CLASS(PvpMapHandler::respawnLocations#list)));
	respawnLocations.get(model::Race::ELYOS)->add(world::WorldPosition::create(mapId, 274.143f, 384.335f, 239.973f, int8_t{14}));
	respawnLocations.get(model::Race::ELYOS)->add(world::WorldPosition::create(mapId, 342.138f, 616.856f, 248.197f, int8_t{35}));
	respawnLocations.put(model::Race::ASMODIANS, runtime::RcArrayList<runtime::Ref<world::WorldPosition>>::create(AION_LOCK_CLASS(PvpMapHandler::respawnLocations#list)));
	respawnLocations.get(model::Race::ASMODIANS)->add(world::WorldPosition::create(mapId, 598.229f, 712.984f, 223.306f, int8_t{73}));
	respawnLocations.get(model::Race::ASMODIANS)->add(world::WorldPosition::create(mapId, 711.403f, 621.797f, 213.276f, int8_t{31}));
}

// Java PvpMapHandler.java: addSupplyPositions
void PvpMapHandler::addSupplyPositions() {
	supplyPositions.clear();
	supplyPositions.add(world::WorldPosition::create(mapId, 709.6463f, 313.6129f, 254.21637f, int8_t{14}));
	supplyPositions.add(world::WorldPosition::create(mapId, 749.5364f, 330.05954f, 233.81584f, int8_t{89}));
	supplyPositions.add(world::WorldPosition::create(mapId, 703.55786f, 292.23004f, 233.81587f, int8_t{119}));
	supplyPositions.add(world::WorldPosition::create(mapId, 612.4221f, 274.82172f, 235.73499f, int8_t{5}));
	supplyPositions.add(world::WorldPosition::create(mapId, 648.80536f, 253.2089f, 235.73445f, int8_t{62}));
	supplyPositions.add(world::WorldPosition::create(mapId, 772.5526f, 411.02084f, 241.0154f, int8_t{90}));
	supplyPositions.add(world::WorldPosition::create(mapId, 709.7187f, 411.01987f, 241.01144f, int8_t{92}));
	supplyPositions.add(world::WorldPosition::create(mapId, 655.7371f, 530.3507f, 226.47437f, int8_t{107}));
	supplyPositions.add(world::WorldPosition::create(mapId, 795.3396f, 532.70593f, 229.58707f, int8_t{115}));
	supplyPositions.add(world::WorldPosition::create(mapId, 646.19354f, 212.37506f, 223.40485f, int8_t{113}));
	supplyPositions.add(world::WorldPosition::create(mapId, 330.28857f, 390.94733f, 226.0986f, int8_t{31}));
	supplyPositions.add(world::WorldPosition::create(mapId, 389.6383f, 625.3936f, 214.52452f, int8_t{1}));
	supplyPositions.add(world::WorldPosition::create(mapId, 280.92044f, 645.77905f, 217.54143f, int8_t{98}));
	supplyPositions.add(world::WorldPosition::create(mapId, 452.21927f, 515.92175f, 223.16016f, int8_t{49}));
	supplyPositions.add(world::WorldPosition::create(mapId, 705.30096f, 654.2082f, 206.6876f, int8_t{48}));
	supplyPositions.add(world::WorldPosition::create(mapId, 513.3479f, 462.09848f, 216.95465f, int8_t{13}));
	supplyPositions.add(world::WorldPosition::create(mapId, 629.89185f, 860.046f, 190.88751f, int8_t{100}));
	supplyPositions.add(world::WorldPosition::create(mapId, 677.7658f, 714.2354f, 178.125f, int8_t{43}));
	supplyPositions.add(world::WorldPosition::create(mapId, 493.3107f, 764.61127f, 200.02097f, int8_t{2}));
	supplyPositions.add(world::WorldPosition::create(mapId, 726.9365f, 328.25638f, 254.21623f, int8_t{73}));
	supplyPositions.add(world::WorldPosition::create(mapId, 640.6047f, 413.0314f, 243.93956f, int8_t{103}));
}

// Java PvpMapHandler.java: addKeymasterPositions
void PvpMapHandler::addKeymasterPositions() {
	keymasterPositions.clear();
	keymasterPositions.add(world::WorldPosition::create(mapId, 354.27307f, 497.8957f, 239.26987f, int8_t{95}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 419.7437f, 769.88983f, 205.1365f, int8_t{119}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 259.40518f, 736.55725f, 201.33997f, int8_t{25}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 793.8396f, 774.8803f, 200.86058f, int8_t{70}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 604.94684f, 900.01465f, 195.53622f, int8_t{94}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 820.80194f, 606.7696f, 239.70268f, int8_t{82}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 552.9182f, 414.02127f, 222.76308f, int8_t{19}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 395.32816f, 272.79468f, 253.375f, int8_t{108}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 717.7881f, 320.78925f, 233.5026f, int8_t{102}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 590.50385f, 506.48993f, 217.75f, int8_t{57}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 667.1276f, 278.3438f, 225.69778f, int8_t{32}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 817.77606f, 371.4041f, 243.45387f, int8_t{48}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 781.83466f, 357.42792f, 230.98207f, int8_t{52}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 630.30853f, 263.7316f, 238.48415f, int8_t{33}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 490.17584f, 667.2109f, 221.4411f, int8_t{61}));
	keymasterPositions.add(world::WorldPosition::create(mapId, 423.2521f, 629.2255f, 214.52452f, int8_t{64}));
}

// Java PvpMapHandler.java: addTreasurePositions
void PvpMapHandler::addTreasurePositions() {
	treasurePositions.clear();
	treasurePositions.add(world::WorldPosition::create(mapId, 644.363f, 222.8753f, 238.07552f, int8_t{20}));
	treasurePositions.add(world::WorldPosition::create(mapId, 822.94727f, 370.6611f, 243.34569f, int8_t{73}));
	treasurePositions.add(world::WorldPosition::create(mapId, 565.6456f, 396.60397f, 228.94838f, int8_t{26}));
	treasurePositions.add(world::WorldPosition::create(mapId, 778.3336f, 787.82886f, 198.75298f, int8_t{83}));
	treasurePositions.add(world::WorldPosition::create(mapId, 599.0367f, 567.8311f, 214.96388f, int8_t{25}));
	treasurePositions.add(world::WorldPosition::create(mapId, 614.5359f, 886.3159f, 193.82806f, int8_t{78}));
	treasurePositions.add(world::WorldPosition::create(mapId, 436.56647f, 754.596f, 202.92058f, int8_t{17}));
	treasurePositions.add(world::WorldPosition::create(mapId, 412.67242f, 645.81995f, 214.52452f, int8_t{92}));
	treasurePositions.add(world::WorldPosition::create(mapId, 353.569f, 504.86948f, 239.26987f, int8_t{92}));
	treasurePositions.add(world::WorldPosition::create(mapId, 567.02625f, 595.80725f, 209.19331f, int8_t{109}));
	treasurePositions.add(world::WorldPosition::create(mapId, 647.5166f, 543.63727f, 222.8279f, int8_t{67}));
	treasurePositions.add(world::WorldPosition::create(mapId, 803.04175f, 604.1839f, 239.5659f, int8_t{92}));
	treasurePositions.add(world::WorldPosition::create(mapId, 676.91064f, 785.8807f, 181.20055f, int8_t{83}));
	treasurePositions.add(world::WorldPosition::create(mapId, 585.99786f, 662.6794f, 211.93208f, int8_t{40}));
	treasurePositions.add(world::WorldPosition::create(mapId, 575.78815f, 850.86847f, 188.95987f, int8_t{105}));
	treasurePositions.add(world::WorldPosition::create(mapId, 464.98325f, 733.43646f, 212.67583f, int8_t{95}));
	treasurePositions.add(world::WorldPosition::create(mapId, 248.65541f, 756.76154f, 201.36113f, int8_t{8}));
	treasurePositions.add(world::WorldPosition::create(mapId, 373.7291f, 327.44672f, 228.25072f, int8_t{7}));
	treasurePositions.add(world::WorldPosition::create(mapId, 447.88635f, 257.91846f, 246.49289f, int8_t{26}));
	treasurePositions.add(world::WorldPosition::create(mapId, 636.1489f, 422.67554f, 242.47498f, int8_t{104}));
	treasurePositions.add(world::WorldPosition::create(mapId, 587.17395f, 507.7597f, 217.75f, int8_t{106}));
	treasurePositions.add(world::WorldPosition::create(mapId, 521.03375f, 542.86005f, 214.33388f, int8_t{70}));
	treasurePositions.add(world::WorldPosition::create(mapId, 726.05597f, 437.2465f, 229.625f, int8_t{76}));
	treasurePositions.add(world::WorldPosition::create(mapId, 731.07806f, 272.2283f, 233.4975f, int8_t{30}));
	treasurePositions.add(world::WorldPosition::create(mapId, 775.14264f, 300.74243f, 233.49748f, int8_t{63}));
	treasurePositions.add(world::WorldPosition::create(mapId, 735.2953f, 248.73598f, 253.43423f, int8_t{44}));
	treasurePositions.add(world::WorldPosition::create(mapId, 786.8716f, 291.9434f, 253.43422f, int8_t{46}));
}

// Java PvpMapHandler.java: getZoneNameL10nId
int32_t PvpMapHandler::getZoneNameL10nId(std::string_view zoneName) {
	if (zoneName == "ANCILLARY_SENTRY_POST_301220000")
	return 404085;
	if (zoneName == "ARTILLERY_COMMAND_CENTER_301220000")
	return 404088;
	if (zoneName == "ASSAULT_COMMAND_CENTER_301220000")
	return 404090;
	if (zoneName == "AXIAL_SENTRY_POST_301220000")
	return 404084;
	if (zoneName == "CENTRAL_SUPPLY_BASE_301220000")
	return 404086;
	if (zoneName == "HEADQUARTERS_301220000")
	return 404092;
	if (zoneName == "HEADQUARTERS_ANNEX_301220000")
	return 404093;
	if (zoneName == "HOLY_GROUND_OF_RESURRECTION_301220000")
	return 404094;
	if (zoneName == "MILITARY_SUPPLY_BASE_2_301220000")
	return 404089;
	if (zoneName == "PASHID_ARMY_ENCAMPMENT_301220000")
	return 404083;
	if (zoneName == "PERIPHERAL_SUPPLY_BASE_301220000")
	return 404087;
	if (zoneName == "SIEGE_BASE_301220000")
	return 404091;
	if (zoneName == "THE_ETERNAL_BASTION_301220000")
	return 404082;
	if (zoneName == "UNDERGROUND_WATERWAY_1_301220000")
	return 404095;
	return 0;
}

} // namespace aion::gameserver::custom::pvpmap
