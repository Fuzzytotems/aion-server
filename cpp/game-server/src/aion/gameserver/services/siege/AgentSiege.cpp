#include "aion/gameserver/services/siege/AgentSiege.h"

#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/AgentLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/mail/SiegeResult.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeException.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::services::siege {

using model::siege::SiegeModType;
using model::siege::SiegeRace;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

AgentSiege::AgentSiege(model::siege::AgentLocation& siegeLocation) : Siege(siegeLocation) {
}

runtime::Ref<AgentSiege> AgentSiege::create(model::siege::AgentLocation& siegeLocation) {
	return runtime::makeRef<AgentSiege>(siegeLocation);
}

runtime::Ptr<model::siege::AgentLocation> AgentSiege::getSiegeLocation() const {
	return runtime::cast<model::siege::AgentLocation>(Siege::getSiegeLocation());
}

// Java AgentSiege.java:44-49
void AgentSiege::onSiegeStart() {
	utils::PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_GODELITE_TIME_01());
	getSiegeLocation()->setVulnerable(true);
	delayStart();
}

// Java AgentSiege.java:51-64; lambda at AgentSiege.java:52 (fieldmap key siege.AgentSiege@L52:44): the self-rescheduling start chain
void AgentSiege::delayStart() {
	utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		startProgress.set(static_cast<int8_t>(startProgress.get() + 1));
		if (startProgress.get() == 5) {
			utils::PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_GODELITE_TIME_02());
		} else if (startProgress.get() >= 10) {
			broadcastAgentSpawn();
			distributeQuest();
			spawnSiegeNpcs(); // Should initialize Agents and their flags
			return;           // Interrupts the task
		}
		delayStart();
	}, 60000);
}

// Java AgentSiege.java:66-79
void AgentSiege::onSiegeFinish() {
	utils::PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_GODELITE_TIME_03());
	getSiegeLocation()->setVulnerable(false);
	despawnSiegeNpcs();
	const std::optional<SiegeRace> winnerRace = winner.get();
	if (!winnerRace)
		return;
	const model::base::BaseOccupier winnerType = *winnerRace == SiegeRace::ELYOS ? model::base::BaseOccupier::ELYOS : model::base::BaseOccupier::ASMODIANS;
	BaseService::getInstance().capture(6113, winnerType);
	const SiegeRace looser = *winnerRace == SiegeRace::ELYOS ? SiegeRace::ASMODIANS : SiegeRace::ELYOS;
	sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(*winnerRace), mail::SiegeResult::OCCUPY);
	sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(looser), mail::SiegeResult::FAIL);
}

// Java AgentSiege.java:81-85
void AgentSiege::broadcastAgentSpawn() {
	runtime::Ptr<world::WorldMapInstance> levinshorWorldInstance = world::World::getInstance().getWorldMap(600100000)->getMainWorldMapInstance();
	if (levinshorWorldInstance)
		utils::PacketSendUtility::broadcastToMap(*levinshorWorldInstance, SM_SYSTEM_MESSAGE::STR_MSG_LDF4_Advance_GodElite());
}

// Java AgentSiege.java:87-97
void AgentSiege::distributeQuest() {
	for (const runtime::Ptr<model::gameobjects::player::Player>& player :
		world::World::getInstance().getWorldMap(600100000)->getMainWorldMapInstance()->getPlayersInside()) {
		if (player->isInsideZone(world::zone::ZoneName::get("DRAGON_LORDS_SHRINE_600100000")) ||
			player->isInsideZone(world::zone::ZoneName::get("FLAMEBERTH_DOWNS_600100000"))) {
			const int32_t questId = player->getRace() == model::Race::ELYOS ? 13744 : 23744;
			runtime::Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(questId);
			if (!qs || qs->isStartable()) {
				runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, *player, questId);
				QuestService::startQuest(*env);
			}
		}
	}
}

// Java AgentSiege.java:99-113
void AgentSiege::spawnSiegeNpcs() {
	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* siegeSpawns =
		dataholders::DataManager::SPAWNS_DATA->getSiegeSpawnsByLocId(getSiegeLocationId());
	if (siegeSpawns == nullptr)
		return;
	for (const runtime::Ref<model::templates::spawns::SpawnGroup>& group : *siegeSpawns) {
		for (const runtime::Ptr<model::templates::spawns::SpawnTemplate>& template_ : group->getSpawnTemplates().snapshot()) {
			runtime::Ptr<model::templates::spawns::siegespawns::SiegeSpawnTemplate> siegetemplate =
				runtime::cast<model::templates::spawns::siegespawns::SiegeSpawnTemplate>(template_);
			if (siegetemplate->getSiegeRace() == SiegeRace::BALAUR && siegetemplate->getSiegeModType() == SiegeModType::SIEGE) {
				runtime::Ptr<model::gameobjects::siege::SiegeNpc> npc =
					runtime::cast<model::gameobjects::siege::SiegeNpc>(spawnengine::SpawnEngine::spawnObject(*siegetemplate, 1));
				if (npc->getSpawn()->getHandlerType() == spawnengine::SpawnHandlerType::BOSS)
					initNpc(*npc);
			}
		}
	}
}

// Java AgentSiege.java:115-121
void AgentSiege::despawnSiegeNpcs() {
	std::vector<runtime::Ptr<model::gameobjects::siege::SiegeNpc>> npcs = world::World::getInstance().getLocalSiegeNpcs(getSiegeLocationId());
	for (const runtime::Ptr<model::gameobjects::siege::SiegeNpc>& npc : npcs) {
		if (npc)
			npc->getController().deleteIfAliveOrCancelRespawn();
	}
}

// Java AgentSiege.java:123-139
void AgentSiege::initNpc(model::gameobjects::siege::SiegeNpc& target) {
	switch (target.getRace()) {
		case model::Race::GHENCHMAN_LIGHT:
			if (veille.get())
				throw SiegeException("Tried to init Veille twice!");
			veille.set(runtime::Ref<model::gameobjects::siege::SiegeNpc>(target));
			break;
		case model::Race::GHENCHMAN_DARK:
			if (masta.get())
				throw SiegeException("Tried to init Mastarius twice!");
			masta.set(runtime::Ref<model::gameobjects::siege::SiegeNpc>(target));
			break;
		default:
			throw SiegeException("Tried to init a npc with not supported TemplateType " + std::string(xml::enumName(target.getNpcTemplateType())) +
				" for agent fight!");
	}
}

// Java AgentSiege.java:141-143
void AgentSiege::setWinnerRace(SiegeRace race) {
	winner.set(race);
}

// Java AgentSiege.java:145-148
bool AgentSiege::isEndless() {
	return false;
}

// Java AgentSiege.java:150-155
void AgentSiege::onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) {
	if (startProgress.get() >= 10 && getSiegeLocation()->isVulnerable() &&
		(player.isInsideZone(world::zone::ZoneName::get("FLAMEBERTH_DOWNS_600100000")) ||
			player.isInsideZone(world::zone::ZoneName::get("DRAGON_LORDS_SHRINE_600100000"))))
		getSiegeCounter()->addAbyssPoints(player, abyssPoints);
}

AgentSiege::~AgentSiege() = default;

} // namespace aion::gameserver::services::siege
