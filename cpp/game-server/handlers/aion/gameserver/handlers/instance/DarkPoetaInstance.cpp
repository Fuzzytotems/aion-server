#include "aion/gameserver/handlers/instance/DarkPoetaInstance.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/instance/InstanceProgressionTypeInfo.h"
#include "aion/gameserver/model/instance/instancescore/DarkPoetaScore.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/DarkPoetaScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(DarkPoetaInstance, 300040000);

// Java DarkPoetaInstance.java:41-43
DarkPoetaInstance::DarkPoetaInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<DarkPoetaInstance> DarkPoetaInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DarkPoetaInstance>(instance);
}

// Java DarkPoetaInstance.java:45-78
void DarkPoetaInstance::onDie(Npc& npc) {
	runtime::Ptr<Creature> master = npc.getMaster();
	if (runtime::as<Player>(master))
		return;

	int32_t npcId = npc.getNpcId();
	int32_t points = calculatePointsReward(npc);
	if (model::instance::isStartProgress(instanceReward.get()->getInstanceProgressionType()) && !excludedNpcs.contains(npcId)) {
		instanceReward.get()->addNpcKill();
		instanceReward.get()->addPoints(points);
		sendPacket(runtime::Ptr<Npc>(npc), points);
	}
	switch (npcId) {
		case 214895: // Main Power Generator
		case 214896: // Auxiliary Power Generator
		case 214897: // Emergency Generator
			if (killedGenerators.incrementAndGet() == 3)
				spawn(214904, 275.34537f, 323.02072f, 130.9302f, int8_t{52});
			break;
		case 214904: // Brigade General Anuhart
			instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
			instanceReward.get()->setRank(checkRank(instanceReward.get()->getPoints()));
			sendPacket(nullptr, 0);
			break;
		case 215280: // Tahabata Pyrelord
		case 215281: // Calindi Flamelord
		case 215282: // Vanuka Infernus
		case 215283: // Asaratu Bloodshade
		case 215284: // Chramati Firetail
			spawn(730211, 1171.9467f, 1223.2805f, 145.43983f, int8_t{16}); // Exit
			break;
	}
}

// Java DarkPoetaInstance.java:80-87
int32_t DarkPoetaInstance::getTime() {
	int32_t current = static_cast<int32_t>(commons::utils::currentTimeMillis() - startTime.get());
	switch (instanceReward.get()->getInstanceProgressionType()) {
		case InstanceProgressionType::PREPARING:
			return 120000 - current;
		case InstanceProgressionType::START_PROGRESS:
		case InstanceProgressionType::END_PROGRESS:
			return 14400000 - current;
		default:
			return 0;
	}
}

// Java DarkPoetaInstance.java:89-93
void DarkPoetaInstance::sendPacket(runtime::Ptr<Npc> npc, int32_t points) {
	if (npc != nullptr)
		PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(npc->getObjectTemplate()->getL10n(), points));
	runtime::Ref<DarkPoetaScoreWriter> writer = DarkPoetaScoreWriter::create(*instanceReward.get());
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java DarkPoetaInstance.java:95-116 (Java's cast of the portal to Npc: 700478 is an npc)
int32_t DarkPoetaInstance::checkRank(int32_t totalPoints) {
	int32_t timeRemain = getTime();
	int32_t rank = 8;
	if (timeRemain > 7200000 && totalPoints >= 17817) {
		spawn(215280, 1189.0f, 1244.0f, 141.0f, int8_t{76});
		rank = 1;
	} else if (timeRemain > 5400000 && totalPoints >= 15219) {
		spawn(215281, 1189.0f, 1244.0f, 141.0f, int8_t{76});
		rank = 2;
	} else if (timeRemain > 3600000 && totalPoints > 10913) {
		spawn(215282, 1189.0f, 1244.0f, 141.0f, int8_t{76});
		rank = 3;
	} else if (timeRemain > 1800000 && totalPoints > 6656) {
		spawn(215283, 1189.0f, 1244.0f, 141.0f, int8_t{76});
		rank = 4;
	} else if (timeRemain > 1) {
		spawn(215284, 1189.0f, 1244.0f, 141.0f, int8_t{76});
		rank = 5;
	}
	schedulePortalDespawn(*runtime::cast<Npc>(spawn(700478, 298.24423f, 316.21954f, 133.29759f, int8_t{56})));
	return rank;
}

// Java DarkPoetaInstance.java:118-120
void DarkPoetaInstance::schedulePortalDespawn(Npc& portal) {
	ThreadPoolManager::getInstance().schedule({this}, [portal = runtime::Ref<Npc>(portal)] { portal->getController().delete_(); }, 180000);
}

// Java DarkPoetaInstance.java:122-242 (Java's switch on a null rating is its NullPointerException; the race is never null: its default is
// NONE in both languages)
int32_t DarkPoetaInstance::calculatePointsReward(Npc& npc) {
	int32_t pointsReward = 0;

	// Usually calculated by npcRank
	std::optional<model::templates::npc::NpcRating> rating = npc.getObjectTemplate()->getRating();
	if (!rating)
		throw runtime::NullPointerException("DarkPoetaInstance: the npc's rating");
	switch (*rating) {
		case model::templates::npc::NpcRating::HERO:
			switch (npc.getObjectTemplate()->getHpGauge()) {
				case 21:
					pointsReward = 786;
					break;
				default:
					pointsReward = 300;
			}
			break;
		default:
			switch (model::getRaceId(npc.getObjectTemplate()->getRace())) {
				case 22: // UNDEAD
					pointsReward = 12;
					break;
				case 9: // BROWNIE
					pointsReward = 18;
					break;
				case 6: // LIZARDMAN
					pointsReward = 24;
					break;
				case 8: // NAGA
				case 18: // DRAGON
				case 24: // MAGICALnpc
					pointsReward = 30;
					break;
				default:
					if (npc.getNpcId() != 281178)
						pointsReward = 11;
					break;
			}
	}

	// Special npcs
	switch (npc.getNpcId()) {
		// Drana
		case 700520:
			pointsReward = 52;
			break;
		// Walls
		case 700517:
		case 700518:
		case 700556:
		case 700558:
			pointsReward = 156;
			break;
		// Mutated Fungie
		case 214885:
			pointsReward = 21;
			break;
		// Named1
		case 214841:
			pointsReward = -209;
			break;
		case 281116:
			pointsReward = 1241;
			break;
		case 215431:
			pointsReward = 208;
			break;
		// Named2
		case 215429:
		case 215430:
			pointsReward = 190;
			break;
		case 214842:
		case 215432:
			pointsReward = 357;
			break;
		// Named3
		case 214871:
		case 215386:
		case 215428:
			pointsReward = 204;
			break;
		// Marabata
		case 214849:
		case 214850:
		case 214851:
			pointsReward = 319;
			break;
		// Generators
		case 214895:
		case 214896:
			pointsReward = 377;
			break;
		case 214897:
			pointsReward = 330;
			break;
		// Atmach
		case 214843:
			pointsReward = 456;
			break;
		// Boss
		case 214864:
		case 214880:
		case 214894:
		case 215387:
		case 215388:
		case 215389:
			pointsReward = 789;
			break;
		case 214904:
			pointsReward = 954;
			break;
	}
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = runtime::cast<model::team::TemporaryPlayerTeam>(instance->getRegisteredTeam());
	if (team != nullptr) {
		if (utils::stats::id(team->getLeaderObject()->getAbyssRank()->getRank()) >= utils::stats::id(utils::stats::AbyssRankEnum::STAR1_OFFICER))
			pointsReward = utils::JavaMath::round(static_cast<float>(pointsReward) * 1.1f);
	}
	return pointsReward;
}

// Java DarkPoetaInstance.java:244-250
void DarkPoetaInstance::onStart(bool manually) {
	instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::START_PROGRESS);
	startTime = commons::utils::currentTimeMillis();
	sendPacket(nullptr, 0);
	if (!manually)
		instance->forEachDoor([](model::gameobjects::StaticDoor& d) { d.setOpen(true); });
}

// Java DarkPoetaInstance.java:252-255
void DarkPoetaInstance::onEnterInstance(Player& player) {
	sendPacket(nullptr, 0);
}

// Java DarkPoetaInstance.java:257-264
void DarkPoetaInstance::onOpenDoor(int32_t doorId) {
	if (doorId == 33) {
		runtime::FutureRef timer = instanceTimer.get();
		if (timer != nullptr && !timer->isCancelled())
			timer->cancel(true);
		onStart(true);
	}
}

// Java DarkPoetaInstance.java:266-270
void DarkPoetaInstance::onInstanceDestroy() {
	runtime::FutureRef timer = instanceTimer.get();
	if (timer != nullptr)
		timer->cancel(false);
}

// Java DarkPoetaInstance.java:272-279
void DarkPoetaInstance::onInstanceCreate() {
	excludedNpcs.addAll(std::vector<int32_t>{700439, 700440, 700441, 700442, 700443, 700444, 700445, 700446, 700447, 281178});
	instanceReward = DarkPoetaScore::create();
	instanceReward.get()->setInstanceProgressionType(InstanceProgressionType::PREPARING);
	startTime = commons::utils::currentTimeMillis();
	instanceTimer = ThreadPoolManager::getInstance().schedule({this}, [this] { onStart(false); }, 121000);
}

// Java DarkPoetaInstance.java:281-285
void DarkPoetaInstance::onGather(Player& player, Gatherable& gatherable) {
	instanceReward.get()->addGather();
	sendPacket(nullptr, 1);
}

// Java DarkPoetaInstance.java:287-291
void DarkPoetaInstance::leaveInstance(Player& player) {
	if (model::instance::isEndProgress(instanceReward.get()->getInstanceProgressionType()))
		TeleportService::moveToInstanceExit(player, mapId, player.getRace());
}

// Java DarkPoetaInstance.java:293-300
void DarkPoetaInstance::onEndCastSkill(Skill& skill) {
	if (skill.getSkillId() != 18130) // Kobold Bomb Effect
		return;
	runtime::Ptr<Npc> camouflageStoneWall = getNpc(700516);
	if (camouflageStoneWall != nullptr && PositionUtil::isInRange(*skill.getEffector(), *camouflageStoneWall, 10))
		camouflageStoneWall->getController().die();
}

} // namespace aion::gameserver::handlers::instance
