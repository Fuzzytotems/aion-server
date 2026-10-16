#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/panesterra/AhserionsFlightSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"

namespace aion::gameserver::services::panesterra::ahserion {

AhserionRaid::AhserionRaid() {
	factions.add(PanesterraFaction::BELUS);
	factions.add(PanesterraFaction::ASPIDA);
	factions.add(PanesterraFaction::ATANATOS);
	factions.add(PanesterraFaction::DISILLON);
}

AhserionRaid::~AhserionRaid() = default;

AhserionRaid& AhserionRaid::getInstance() {
	// Java SingletonHolder. A per-run service stays RefCounted (fieldmap.toml per_run_services): one Ref that is never released.
	static const runtime::Ref<AhserionRaid>* const instance = new runtime::Ref<AhserionRaid>(runtime::makeRef<AhserionRaid>());
	return **instance;
}

// Java AhserionRaid.java:49-54
void AhserionRaid::start() {
	if (isStarted_.compareAndSet(false, true)) {
		spawnRaid();
		startInstanceTimer();
	}
}

// Java AhserionRaid.java:56-62
void AhserionRaid::stop() {
	if (!isStarted_.compareAndSet(true, false))
		return;
	winner.set(nullptr);
	cancelProgressTask();
	cleanUp();
}

// Java AhserionRaid.java:64-75
void AhserionRaid::cleanUp() {
	for (runtime::Ptr<model::gameobjects::VisibleObject> obj : *world::World::getInstance().getWorldMap(400030000)->getMainWorldMapInstance()) {
		if (runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(obj)) {
			if (!player->isStaff())
				teleport::TeleportService::moveToBindLocation(*player);
		} else if (runtime::Ptr<model::gameobjects::StaticDoor> door = runtime::as<model::gameobjects::StaticDoor>(obj)) {
			door->setOpen(false);
		} else if (runtime::as<model::gameobjects::Npc>(obj) != nullptr) {
			obj->getController().delete_();
		}
	}
}

// Java AhserionRaid.java:77-146: the anonymous Runnable (fieldmap key AhserionRaid$1) and its int progress, a shared counter here
void AhserionRaid::startInstanceTimer() {
	runtime::Ref<runtime::Rc<runtime::AtomicInteger>> progress = runtime::Rc<runtime::AtomicInteger>::create(0); // Java: private int progress
	progressTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, progress] {
		const auto send = [this](network::aion::serverpackets::SM_SYSTEM_MESSAGE msg) { sendMsg(msg); }; // sendMsg takes the packet by reference
		switch (progress->incrementAndGet()) {
			case 2:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_01());
				break;
			case 4:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_02());
				break;
			case 8:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_03());
				break;
			case 12:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_04());
				break;
			case 16:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_05());
				break;
			case 18:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_06());
				break;
			case 19:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_07());
				for (uint8_t faction = 0; faction <= static_cast<uint8_t>(PanesterraFaction::PEACE); faction++) // Java: PanesterraFaction.values()
					spawnStage(2, static_cast<PanesterraFaction>(faction)); // spawn mobs 30s before doors are opened
				break;
			case 20:
				checkForIllegalMovement();
				world::World::getInstance().getWorldMap(400030000)->getMainWorldMapInstance()->forEachDoor(
					[](model::gameobjects::StaticDoor& door) { door.setOpen(true); });
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_08());
				break;
			case 30:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_09());
				for (uint8_t faction = 0; faction <= static_cast<uint8_t>(PanesterraFaction::PEACE); faction++)
					spawnStage(3, static_cast<PanesterraFaction>(faction));
				break;
			case 40:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_10());
				break;
			case 50:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_11());
				break;
			case 60:
				forEachTeam([this](PanesterraTeam& team) {
					if (!team.isEliminated())
						spawnStage(4, team.getFaction());
				});
				break;
			case 130:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_13());
				break;
			case 138:
				send(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GAB1_SUB_ALARM_14());
				break;
			case 150:
				forEachTeam([this](PanesterraTeam& team) {
					if (!team.isEliminated())
						sendConsolationReward(team);
				});
				stop();
				break;
			default:
				break;
		}
	}, 30000, 30000));
}

// Java AhserionRaid.java:148-159
void AhserionRaid::checkForIllegalMovement() {
	forEachTeam([](PanesterraTeam& team) {
		runtime::Ptr<world::WorldPosition> startPosition = team.getStartPosition();
		team.forEachMember([&team, &startPosition](model::gameobjects::player::Player& player) {
			if (player.getPosition()->getMapId() == 400030000
				&& !utils::PositionUtil::isInRange(player, startPosition->getX(), startPosition->getY(), startPosition->getZ(), 81.0f)) {
				utils::audit::AuditLogger::log(player, "bugged himself through the " + std::string(xml::enumName(team.getFaction())) + " start door");
				team.movePlayerToStartPosition(player);
			}
		});
	});
}

// Java AhserionRaid.java:161-192
void AhserionRaid::spawnRaid() {
	// spawn Barricades & Tank Fleets
	spawnStage(0, PanesterraFaction::BALAUR);
	spawnStage(180, PanesterraFaction::BALAUR);
	spawnStage(181, PanesterraFaction::BALAUR);
	spawnStage(182, PanesterraFaction::BALAUR);
	spawnStage(183, PanesterraFaction::BALAUR);
	// spawn flags & cannons for all registered teams
	forEachTeam([this](PanesterraTeam& team) {
		spawnStage(0, team.getFaction());
		spawnStage(1, team.getFaction());
	});
	// spawn white flags for not existing teams
	if (PanesterraService::getInstance().getTeam(PanesterraFaction::BELUS) == nullptr) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> template_ =
			spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804106, 282.73f, 289.1f, 687.38f, int8_t{1});
		spawnengine::SpawnEngine::spawnObject(*template_, 1);
	}
	if (PanesterraService::getInstance().getTeam(PanesterraFaction::ASPIDA) == nullptr) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> template_ =
			spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804108, 282.49f, 739.62f, 689.66f, int8_t{1});
		spawnengine::SpawnEngine::spawnObject(*template_, 1);
	}
	if (PanesterraService::getInstance().getTeam(PanesterraFaction::ATANATOS) == nullptr) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> template_ =
			spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804110, 734.06f, 740.75f, 681.16f, int8_t{1});
		spawnengine::SpawnEngine::spawnObject(*template_, 1);
	}
	if (PanesterraService::getInstance().getTeam(PanesterraFaction::DISILLON) == nullptr) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> template_ =
			spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804112, 738.58f, 286.02f, 680.71f, int8_t{1});
		spawnengine::SpawnEngine::spawnObject(*template_, 1);
	}
}

// Java AhserionRaid.java:194-212 (Java's casts (AhserionsFlightSpawnTemplate), (Npc) and (NpcAI))
void AhserionRaid::spawnStage(int32_t stage, PanesterraFaction faction) {
	runtime::Ptr<PanesterraTeam> team = PanesterraService::getInstance().getTeam(faction);
	if (faction != PanesterraFaction::BALAUR && (team == nullptr || team->isEliminated()))
		return;
	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* ahserionSpawns =
		dataholders::DataManager::SPAWNS_DATA->getAhserionSpawnByTeamId(static_cast<int32_t>(faction));
	if (ahserionSpawns == nullptr)
		return;
	for (const runtime::Ref<model::templates::spawns::SpawnGroup>& grp : *ahserionSpawns) {
		for (runtime::Ptr<model::templates::spawns::SpawnTemplate> template_ : grp->getSpawnTemplates().snapshot()) {
			runtime::Ptr<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate> ahserionTemplate =
				runtime::cast<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate>(template_);
			if (ahserionTemplate->getStage() == stage) {
				runtime::Ptr<model::gameobjects::Npc> npc = runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*ahserionTemplate, 1));
				if (npc == nullptr) // Java: npc.getAi() on a failed spawn
					throw runtime::NullPointerException("AhserionRaid: the spawn of stage " + std::to_string(stage));
				ai::manager::WalkManager::startWalking(*runtime::cast<ai::NpcAI>(npc->getAi()));
			}
		}
	}
}

// Java AhserionRaid.java:214-245 (an unknown npc id leaves the faction and template null: Java's NullPointerException in spawnObject)
void AhserionRaid::handleCorridorShieldDestruction(int32_t npcId) {
	if (!isStarted_.get())
		return;
	std::optional<PanesterraFaction> eliminatedFaction;
	runtime::Ref<model::templates::spawns::SpawnTemplate> template_;
	switch (npcId) {
		case 297306:
			eliminatedFaction = PanesterraFaction::BELUS;
			template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804106, 282.73f, 289.1f, 687.38f, int8_t{1});
			break;
		case 297307:
			eliminatedFaction = PanesterraFaction::ASPIDA;
			template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804108, 282.49f, 739.62f, 689.66f, int8_t{1});
			break;
		case 297308:
			eliminatedFaction = PanesterraFaction::ATANATOS;
			template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804110, 734.06f, 740.75f, 681.16f, int8_t{1});
			break;
		case 297309:
			eliminatedFaction = PanesterraFaction::DISILLON;
			template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(400030000, 804112, 738.58f, 286.02f, 680.71f, int8_t{1});
			break;
		default:
			break;
	}
	if (!eliminatedFaction)
		throw runtime::NullPointerException("AhserionRaid: no faction for corridor shield " + std::to_string(npcId));
	runtime::Ptr<PanesterraTeam> eliminatedTeam = PanesterraService::getInstance().handleTeamElimination(*eliminatedFaction);
	if (eliminatedTeam != nullptr)
		sendConsolationReward(*eliminatedTeam);
	deleteNpcs(*eliminatedFaction, npcId + 1);
	spawnengine::SpawnEngine::spawnObject(*template_, 1);
}

// Java AhserionRaid.java:247-251
void AhserionRaid::sendConsolationReward(PanesterraTeam& eliminatedTeam) {
	eliminatedTeam.forEachMember([](model::gameobjects::player::Player& p) {
		mail::SystemMailService::sendMail("Assault Forces", p.getName(), "Raid Announcement", "We lost.", 186000409, 100, 0,
			model::gameobjects::LetterType::NORMAL);
	});
}

// Java AhserionRaid.java:253-270
void AhserionRaid::handleBossKilled(model::gameobjects::Npc& ahserion, PanesterraFaction winnerFaction) {
	winner.set(runtime::Ref<PanesterraTeam>(PanesterraService::getInstance().getTeam(winnerFaction)));
	runtime::Ref<PanesterraTeam> winnerTeam = winner.get();
	if (winnerTeam == nullptr || winnerTeam->isEliminated()) {
		// something went wrong, remove all players from the map
		commons::logging::LoggerFactory::getLogger("AhserionRaid").warn("Ahserion got killed but winnerTeam is missing or eliminated. Skipping rewards.");
		stop();
		return;
	}
	cancelProgressTask();
	for (PanesterraFaction faction : factions.snapshot()) {
		if (faction != winnerFaction)
			PanesterraService::getInstance().handleTeamElimination(faction);
	}
	ahserion.getPosition()->getWorldMapInstance()->forEachNpc([](model::gameobjects::Npc& npc) { npc.getController().deleteIfAliveOrCancelRespawn(); });
	spawnStage(10, winnerFaction); // Quest Npc "Pasha"
	utils::ThreadPoolManager::getInstance().schedule({this}, [this] { stop(); }, 900000); // 15min
}

// Java AhserionRaid.java:272-274
void AhserionRaid::sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg) {
	utils::PacketSendUtility::broadcastToMap(*world::World::getInstance().getWorldMap(400030000)->getMainWorldMapInstance(), msg);
}

// Java AhserionRaid.java:276-285
void AhserionRaid::deleteNpcs(PanesterraFaction eliminatedFaction, int32_t flagToDelete) {
	world::World::getInstance().getWorldMap(400030000)->getMainWorldMapInstance()->forEachNpc([eliminatedFaction, flagToDelete](model::gameobjects::Npc& npc) {
		if (npc.getNpcId() == flagToDelete || (!npc.isFlag() && (npc.getSpawn()->getStaticId() < 180 || npc.getSpawn()->getStaticId() > 183))) {
			runtime::Ptr<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate> template_ =
				runtime::as<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate>(npc.getSpawn());
			if (!npc.isDead() && template_ != nullptr) {
				if (template_->getFaction() == eliminatedFaction)
					npc.getController().delete_();
			}
		}
	});
}

// Java AhserionRaid.java:287-293
void AhserionRaid::forEachTeam(const std::function<void(PanesterraTeam&)>& consumer) {
	for (PanesterraFaction faction : factions.snapshot()) {
		runtime::Ptr<PanesterraTeam> team = PanesterraService::getInstance().getTeam(faction);
		if (team != nullptr)
			consumer(*team);
	}
}

// Java AhserionRaid.java:295-298
void AhserionRaid::cancelProgressTask() {
	runtime::FutureRef task = progressTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java AhserionRaid.java:300-302
bool AhserionRaid::isStarted() {
	return isStarted_.get();
}

} // namespace aion::gameserver::services::panesterra::ahserion
