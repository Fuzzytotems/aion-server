#include "aion/gameserver/handlers/instance/crucible/CrucibleChallengeInstance.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/instance/StageTypeInfo.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/CrucibleScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_STAGE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance::crucible {

AION_INSTANCE_HANDLER(CrucibleChallengeInstance, 300320000);

// Java CrucibleChallengeInstance.java:45-47
CrucibleChallengeInstance::CrucibleChallengeInstance(WorldMapInstance& instance) : CrucibleInstance(instance) {
}

runtime::Ref<CrucibleChallengeInstance> CrucibleChallengeInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<CrucibleChallengeInstance>(instance);
}

// Java CrucibleChallengeInstance.java:49-54
void CrucibleChallengeInstance::onInstanceCreate() {
	CrucibleInstance::onInstanceCreate();
	sp(205668, 345.77954f, 1662.6697f, 95.25f, int8_t{0}, 0);
	sp(205682, 383.3434f, 1667.2977f, 97.79293f, int8_t{60}, 0);
}

// Java CrucibleChallengeInstance.java:56-64
void CrucibleChallengeInstance::onEnterInstance(Player& player) {
	CrucibleInstance::onEnterInstance(player);
	if (getPlayerReward(player.getObjectId())->isRewarded())
		doReward(player);
	else
		sendPacket(std::nullopt, 0);
	sendEventPacket();
}

// Java CrucibleChallengeInstance.java:66-70
void CrucibleChallengeInstance::sendPacket(std::optional<std::string_view> npcL10n, int32_t points) {
	runtime::Ref<CrucibleScoreWriter> writer = CrucibleScoreWriter::create(*instanceScore.get());
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer));
	if (npcL10n)
		PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(*npcL10n, points));
}

// Java CrucibleChallengeInstance.java:72-74
void CrucibleChallengeInstance::sendEventPacket() {
	PacketSendUtility::broadcastToMap(*instance,
		SM_INSTANCE_STAGE_INFO(2, model::instance::getId(stageType.get()), model::instance::getType(stageType.get())));
}

// Java CrucibleChallengeInstance.java:76-257
void CrucibleChallengeInstance::onDie(Npc& npc) {
	int32_t npcId = npc.getNpcId();
	increasePointsForKill(npc, npc.getAggroList().getMostPlayerDamage());
	switch (npcId) {
		case 217784: // ely
		case 217785: // asmo
			if (++stage1Count == 2) {
				sp(npcId, 342.855f, 1652.6703f, 95.63069f, int8_t{0}, 0);
				sp(npcId, 342.83942f, 1673.2863f, 95.66078f, int8_t{0}, 0);
			} else if (allDeadOrDespawned({npcId}) && changeStage(StageType::START_STAGE_1_ROUND_2, 2000)) {
				sp(217783, 337.82263f, 1662.9073f, 95.27217f, int8_t{0}, 4000);
			}
			break;
		case 217783:
			if (changeStage(StageType::PASS_STAGE_1, 0))
				sp(217758, 347.24026f, 1660.2524f, 95.35922f, int8_t{0}, 0);
			break;
		case 217843:
		case 217845:
			if (changeStage(StageType::START_STAGE_3_ROUND_2, 2000)) {
				sp(217848, 1309.2858f, 1732.6802f, 316.0825f, int8_t{0}, 1000);
				sp(217848, 1306.1871f, 1731.7861f, 316.25168f, int8_t{0}, 1000);
				sp(217848, 1308.4879f, 1734.4999f, 316.0f, int8_t{0}, 1000);
				sp(217848, 1308.67419f, 1736.2063f, 316.0f, int8_t{0}, 1000);
				sp(217848, 1306.9414f, 1736.5365f, 316.0f, int8_t{0}, 1000);
				sp(217848, 1305.2534f, 1735.1603f, 315.94586f, int8_t{0}, 1000);
				sp(217847, 1307.2786f, 1734.3274f, 316.0f, int8_t{0}, 3000);
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					runtime::Ptr<Npc> fieryGomju = getNpc(217847);
					if (fieryGomju != nullptr) {
						runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(*fieryGomju, 19817, 1, fieryGomju);
						if (skill == nullptr) // Java: getSkill(...).useSkill() on null
							throw runtime::NullPointerException("CrucibleChallengeInstance: skill 19817");
						if (skill->useSkill()) { // Brown Bear's Roar
							ThreadPoolManager::getInstance().schedule({this}, [this] {
								std::optional<std::vector<runtime::Ptr<Npc>>> dracunis = getNpcs(217848);
								if (!dracunis) // Java: forEach on getNpcs' null
									throw runtime::NullPointerException("CrucibleChallengeInstance: getNpcs(217848)");
								for (const runtime::Ptr<Npc>& dracuni : *dracunis)
									dracuni->getController().die();
							}, 1000);
						}
					}
				}, 4000);
			}
			break;
		case 217847:
			sp(205676, 1307.6722f, 1732.9865f, 316.07373f, int8_t{6}, 0);
			break;
		case 217788:
		case 217789:
		case 217790:
			if (changeStage(StageType::START_STAGE_4_ROUND_2, 2000))
				sp(217794, 1271.0f, 791.5752f, 436.63998f, int8_t{0}, 6000);
			break;
		case 217791:
		case 217792:
		case 217793:
			if (changeStage(StageType::START_STAGE_4_ROUND_2, 2000))
				sp(217795, 1258.8425f, 237.91522f, 405.3968f, int8_t{0}, 6000);
			break;
		case 217794:
		case 217795:
			if (stageType.get() != StageType::START_BONUS_STAGE_4) {
				changeStage(StageType::PASS_STAGE_4, 0);
				changeStage(StageType::START_BONUS_STAGE_4, 4000);
				if (npcId == 217794)
					sp(217820, 1266.9661f, 791.5348f, 436.64014f, int8_t{0}, 4000);
				else
					sp(217820, 1251.1598f, 237.97736f, 405.3968f, int8_t{0}, 4000);
			}
			break;
		case 217786:
		case 217787:
			break; // just despawn (see below switch)
		case 217807: // ely
		case 217808: // ely
		case 217809: // ely
		case 217810: // ely
		case 217811: // asmo
		case 217812: // asmo
		case 217813: // asmo
		case 217814: // asmo
		case 217815: // ely
		case 217816: // asmo
			if (changeStage(StageType::START_STAGE_5_ROUND_2, 2000)) {
				switch (commons::utils::Rnd::get(1, 3)) {
					case 1:
						sp(217806, 332.3786f, 349.31204f, 96.090935f, int8_t{0}, 4000);
						break;
					case 2:
						sp(218562, 332.3786f, 349.31204f, 96.090935f, int8_t{0}, 4000);
						break;
					case 3:
						sp(218565, 332.3786f, 349.31204f, 96.090935f, int8_t{0}, 4000);
						break;
				}
			}
			break;
		case 217806:
		case 218562:
		case 218565:
			if (changeStage(StageType::PASS_STAGE_5, 0))
				sp(205678, 346.64798f, 349.25586f, 96.090965f, int8_t{0}, 0);
			break;
		case 217819:
			if (changeStage(StageType::PASS_STAGE_6, 0)) {
				ThreadPoolManager::getInstance().schedule({this}, [this, npcRef = runtime::Ref<Npc>(npc)] {
					runtime::Ptr<Player> player = nullptr;
					std::vector<runtime::Ptr<Player>> playersInside = instance->getPlayersInside();
					if (!playersInside.empty()) {
						player = playersInside.front();
					}
					npcRef->getController().delete_();
					if (player != nullptr) {
						runtime::Ptr<questEngine::model::QuestState> qs =
							player->getQuestStateList()->getQuestState(player->getRace() == Race::ASMODIANS ? 28208 : 18208);
						if (qs != nullptr && qs->getStatus() == questEngine::model::QuestStatus::START) {
							if (qs->getQuestVarById(0) == 1 || qs->getQuestVarById(1) == 4) { // 5 x Vanktrist Spacetwine killed
								sp(730459, 1765.7104f, 1281.2388f, 389.11743f, int8_t{0}, 2000);
								return;
							}
						}
					}
					sp(205679, 1765.522f, 1282.1051f, 389.11743f, int8_t{0}, 2000);
				}, 4000);
			}
			return; // return instead of break since despawn is delayed
		case 217837:
		case 217838: {
			runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage();
			if (player != nullptr) {
				if (player->isInsideZone(ZoneName::get("ILLUSION_STADIUM_1_300320000"))) {
					sp(217820, 1251.1598f, 237.97736f, 405.3968f, int8_t{0}, 0);
				} else if (player->isInsideZone(ZoneName::get("ILLUSION_STADIUM_6_300320000"))) {
					sp(217820, 1266.9661f, 791.5348f, 436.64014f, int8_t{0}, 0);
				}
			}
			break;
		}
		case 217800:
		case 217801:
			if (allDeadOrDespawned({217800, 217801}))
				startBonusStage2();
			break;
		case 217802: {
			deleteAliveNpcs({217803});
			runtime::FutureRef timer = bonusTimer.get();
			if (timer == nullptr) // Java: bonusTimer.cancel(false) on null
				throw runtime::NullPointerException("CrucibleChallengeInstance: bonusTimer");
			timer->cancel(false);
			passStage2();
			break;
		}
		case 217797:
		case 217798:
		case 217799:
			if (allDeadOrDespawned({217797, 217798, 217799}))
				startBonusStage2();
			break;
		case 217841:
		case 218561: {
			int32_t spawnNpcId;
			switch (npc.getNpcId()) {
				case 218561:
					spawnNpcId = 218560;
					break;
				case 217841:
					spawnNpcId = 217840;
					break;
				default:
					return;
			}
			if (commons::utils::Rnd::nextBoolean())
				sp(spawnNpcId, 1299.7399f, 1733.5763f, 316.6515f, int8_t{0}, 0);
			else
				sp(spawnNpcId, 1297.627f, 1729.8733f, 316.875f, int8_t{0}, 0);
			break;
		}
		case 217803: {
			if (++dieCount == 5)
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_5Dead_BROADCAST());
			runtime::FutureRef timer = bonusTimer.get();
			if (timer == nullptr) // Java: bonusTimer.isCancelled() on null
				throw runtime::NullPointerException("CrucibleChallengeInstance: bonusTimer");
			if (timer->isCancelled() && allDeadOrDespawned({npcId})) {
				runtime::Ptr<Npc> poppy = getNpc(217802);
				if (poppy != nullptr) {
					passStage2();
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_Succ_BROADCAST());
					sp(218571, poppy->getX(), poppy->getY(), poppy->getZ(), int8_t{0}, 0);
					poppy->getController().delete_();
				}
			}
			break;
		}
		case 217844:
		case 217842: {
			std::optional<std::vector<runtime::Ptr<Npc>>> npcs = getNpcs(npcId);
			if (!npcs) // Java: size() on getNpcs' null
				throw runtime::NullPointerException("CrucibleChallengeInstance: getNpcs");
			if (npcs->size() == 2)
				ThreadPoolManager::getInstance().schedule({this}, [this, npcId] { startStage3Round1_1(npcId); }, 5000);
			break;
		}
		case 218185:
			deleteAliveNpcs({218190, 218191});
			sp(730460, 1759.2914f, 1786.37f, 389.11713f, int8_t{96}, 0);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(npc), 342495);
			break;
		default:
			return; // don't despawn
	}
	npc.getController().delete_();
}

// Java CrucibleChallengeInstance.java:259-328
void CrucibleChallengeInstance::increasePointsForKill(Npc& npc, runtime::Ptr<Player> killer) {
	if (killer == nullptr)
		return;
	int32_t points = 0;
	switch (npc.getNpcId()) {
		case 217784:
		case 217785:
			points = 120;
			break;
		case 217786:
		case 217787:
			points = 200;
			break;
		case 217797:
		case 217798:
		case 217799:
			points = 1100;
			break;
		case 217788:
		case 217789:
		case 217790:
		case 217791:
		case 217792:
		case 217793:
			points = 1300;
			break;
		case 217843:
		case 217845:
			points = 1400;
			break;
		case 217783:
			points = 1600;
			break;
		case 217800:
		case 217801:
			points = 1650;
			break;
		case 217807: // ely
		case 217808: // ely
		case 217809: // ely
		case 217810: // ely
		case 217811: // asmo
		case 217812: // asmo
		case 217813: // asmo
		case 217814: // asmo
		case 217815: // ely
		case 217816: // asmo
			points = 2500;
			break;
		case 217847:
			points = 4600;
			break;
		case 217795:
		case 217794:
			points = 5000;
			break;
		case 217806:
		case 218562:
		case 218565:
			points = 5800;
			break;
		case 217819:
			points = 7200;
			break;
	}
	if (points != 0) {
		getPlayerReward(killer->getObjectId())->addPoints(points);
		sendPacket(npc.getObjectTemplate()->getL10n(), points);
	}
}

// Java CrucibleChallengeInstance.java:330-338
bool CrucibleChallengeInstance::allDeadOrDespawned(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : instance->getNpcs()) {
		for (int32_t npcId : npcIds) {
			if (npc->getNpcId() == npcId && !npc->isDead())
				return false;
		}
	}
	return true;
}

// Java CrucibleChallengeInstance.java:340-350
void CrucibleChallengeInstance::startStage3Round1_1(int32_t npcId) {
	int32_t bossId;
	switch (npcId) {
		case 217844:
			bossId = 217845;
			break;
		case 217842:
			bossId = 217843;
			break;
		default:
			bossId = 0;
			break;
	}
	if (getNpc(bossId) != nullptr)
		return;
	sp(bossId, 1287.6239f, 1724.2721f, 317.1485f, int8_t{6}, 2000);
	deleteAliveNpcs({npcId, 217840, 218560, 218561, 217841});
}

// Java CrucibleChallengeInstance.java:352-366
void CrucibleChallengeInstance::doReward(Player& player) {
	runtime::Ptr<CruciblePlayerReward> playerReward = getPlayerReward(player.getObjectId());
	if (playerReward == nullptr) {
		return;
	}
	float reward = 0.01f * static_cast<float>(playerReward->getPoints()) + 120;
	if (!playerReward->isRewarded()) {
		playerReward->setRewarded();
		ItemService::addItem(player, 186000130, static_cast<int32_t>(reward));
		playerReward->setInsignia(static_cast<int32_t>(reward));
		instanceScore.get()->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
	}
	runtime::Ref<CrucibleScoreWriter> writer = CrucibleScoreWriter::create(*instanceScore.get());
	PacketSendUtility::sendPacket(player, SM_INSTANCE_SCORE(instance->getMapId(), *writer));
}

// Java CrucibleChallengeInstance.java:368-389
void CrucibleChallengeInstance::startBonusStage2() {
	if (!changeStage(StageType::PASS_STAGE_2, 0))
		return;

	ThreadPoolManager::getInstance().schedule({this}, [this] { changeStage(StageType::START_BONUS_STAGE_2, 1000); }, 2000);

	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isInstanceDestroyed) {
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_START_BROADCAST());
			runtime::Ref<SpawnTemplate> spawnTemplate = SpawnEngine::newSingleTimeSpawn(mapId, 217802, 1780.5665f, 307.40463f, 469.25f, int8_t{0});
			spawnTemplate->setWalkerId("3003200001");
			SpawnEngine::spawnObject(*spawnTemplate, instance->getInstanceId());
		}
	}, 6000);

	bonusTimer = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		spawnCount++;
		spawnBonus2();
		if (spawnCount == 10)
			bonusTimer.get()->cancel(false);
	}, 10000, 4000);
}

// Java CrucibleChallengeInstance.java:391-404
void CrucibleChallengeInstance::spawnBonus2() {
	switch (spawnCount) {
		case 1:
			sp(217803, 1789.923f, 310.853f, 469.25f, int8_t{0}, 0);
			break;
		case 2:
			sp(217803, 1780.9126f, 315.63382f, 469.25f, int8_t{0}, 0);
			break;
		case 3:
			sp(217803, 1784.4752f, 300.1912f, 469.25f, int8_t{0}, 0);
			break;
		case 4:
			sp(217803, 1793.2847f, 312.59842f, 469.25f, int8_t{0}, 0);
			break;
		case 5:
			sp(217803, 1777.6609f, 316.1443f, 469.25f, int8_t{0}, 0);
			break;
		case 6:
			sp(217803, 1776.5625f, 299.30347f, 469.25f, int8_t{0}, 0);
			break;
		case 7:
			sp(217803, 1793.6469f, 301.40973f, 469.25f, int8_t{0}, 0);
			break;
		case 8:
			sp(217803, 1797.4891f, 310.50418f, 469.25f, int8_t{0}, 0);
			break;
		case 9:
			sp(217803, 1782.552f, 319.43973f, 469.25f, int8_t{0}, 0);
			break;
		case 10:
			sp(217803, 1776.1577f, 305.80396f, 469.25f, int8_t{0}, 0);
			break;
	}
}

// Java CrucibleChallengeInstance.java:406-412
void CrucibleChallengeInstance::onInstanceDestroy() {
	CrucibleInstance::onInstanceDestroy();
	runtime::FutureRef timer = bonusTimer.get();
	if (timer != nullptr) {
		timer->cancel(false);
	}
}

// Java CrucibleChallengeInstance.java:414-417
void CrucibleChallengeInstance::passStage2() {
	if (getNpc(205675) == nullptr) // record keeper
		sp(205675, 1784.5883f, 306.98645f, 469.25f, int8_t{0}, 0);
}

// Java CrucibleChallengeInstance.java:419-440
bool CrucibleChallengeInstance::onDie(Player& player, Creature& lastAttacker) {
	CrucibleInstance::onDie(player, lastAttacker);
	int32_t place = 0;
	if (isInZone(ZoneName::get("ILLUSION_STADIUM_4_300320000"), player)) { // stage 1
		place = 1;
	} else if (isInZone(ZoneName::get("ILLUSION_STADIUM_8_300320000"), player)) { // stage 2
		place = 2;
	} else if (isInZone(ZoneName::get("ILLUSION_STADIUM_5_300320000"), player)) { // stage 3
		place = 3;
	} else if (isInZone(ZoneName::get("ILLUSION_STADIUM_1_300320000"), player)) { // stage 4
		place = 4;
	} else if (isInZone(ZoneName::get("ILLUSION_STADIUM_6_300320000"), player)) { // stage 4
		place = 5;
	} else if (isInZone(ZoneName::get("ILLUSION_STADIUM_2_300320000"), player)) { // stage 5
		place = 6;
	} else if (isInZone(ZoneName::get("ILLUSION_STADIUM_7_300320000"), player)) { // stage 6
		place = 7;
	}
	getPlayerReward(player.getObjectId())->setSpawnPosition(place);
	return true;
}

// Java CrucibleChallengeInstance.java:442-460
bool CrucibleChallengeInstance::onReviveEvent(Player& player) {
	CrucibleInstance::onReviveEvent(player);
	switch (getPlayerReward(player.getObjectId())->getSpawnPosition()) {
		case 1:
			teleport(player, 380.35417f, 1663.3583f, 97.60156f, int8_t{0});
			break;
		case 2:
			teleport(player, 1819.8119f, 304.92932f, 469.4142f, int8_t{0});
			break;
		case 3:
			teleport(player, 1354.9386f, 1748.1531f, 318.6173f, int8_t{70});
			break;
		case 4:
			teleport(player, 1294.1417f, 234.49684f, 406.0368f, int8_t{0});
			break;
		case 5:
			teleport(player, 1307.3776f, 790.7324f, 437.29678f, int8_t{0});
			break;
		case 6:
			teleport(player, 381.7477f, 346.63913f, 96.74763f, int8_t{0});
			break;
		case 7:
			teleport(player, 1750.2677f, 1253.5453f, 389.11765f, int8_t{10});
			break;
		default:
			InstanceService::destroyInstance(*player.getPosition()->getWorldMapInstance());
			break;
	}

	if (player.getInventory().getFirstItemByItemId(186000134) == nullptr) {
		doReward(player);
	}
	return true;
}

// Java CrucibleChallengeInstance.java:462-465
void CrucibleChallengeInstance::leaveInstance(Player& player) {
	InstanceService::destroyInstance(*player.getPosition()->getWorldMapInstance());
}

// Java CrucibleChallengeInstance.java:467-470
void CrucibleChallengeInstance::onStopTraining(Player& player) {
	doReward(player);
}

// Java CrucibleChallengeInstance.java:472-578
void CrucibleChallengeInstance::onChangeStage(StageType type) {
	if (!changeStage(type, 2000))
		return;
	int32_t npcId = 0, barrelId = 0;
	runtime::Ptr<Player> player = nullptr;
	std::vector<runtime::Ptr<Player>> playersInside = instance->getPlayersInside();
	if (!playersInside.empty()) {
		player = playersInside.front();
	}
	switch (stageType.get()) {
		case StageType::START_STAGE_1_ROUND_1:
			if (player != nullptr) {
				switch (player->getRace()) {
					case Race::ELYOS:
						npcId = 217784;
						break;
					case Race::ASMODIANS:
						npcId = 217785;
						break;
					default:
						break;
				}
				sp(npcId, 334.85098f, 1657.8495f, 95.77262f, int8_t{0}, 4000);
				sp(npcId, 334.74506f, 1668.7478f, 95.67427f, int8_t{0}, 4000);
				sp(npcId, 350.63846f, 1663.84f, 95.385f, int8_t{0}, 4000);
			}
			break;
		case StageType::START_STAGE_2_ROUND_1:
			switch (commons::utils::Rnd::get(1, 2)) {
				case 1:
					sp(217800, 1779.9993f, 305.76697f, 469.25f, int8_t{30}, 4000);
					sp(217801, 1779.9845f, 308.82013f, 469.25f, int8_t{90}, 4000);
					break;
				case 2:
					sp(217797, 1771.5549f, 307.09192f, 469.25f, int8_t{30}, 4000);
					sp(217798, 1775.9717f, 317.69516f, 469.25f, int8_t{90}, 4000);
					sp(217799, 1775.9646f, 296.3559f, 469.25f, int8_t{90}, 4000);
					break;
			}
			break;
		case StageType::START_STAGE_3_ROUND_1:
			switch (commons::utils::Rnd::get(1, 2)) {
				case 1:
					sendMsg(SM_SYSTEM_MESSAGE::STR_CHAT_IDArena_Solo_S4_System2());
					npcId = 217844;
					barrelId = 218560;
					break;
				case 2:
					sendMsg(SM_SYSTEM_MESSAGE::STR_CHAT_IDArena_Solo_S4_System1());
					npcId = 217842;
					barrelId = 217840;
					break;
			}
			sp(barrelId, 1302.4689f, 1732.1583f, 316.4486f, int8_t{0}, 2000);
			sp(npcId, 1288.6692f, 1730.5212f, 316.85333f, int8_t{0}, 4000);
			sp(npcId, 1296.7896f, 1726.1091f, 316.82837f, int8_t{0}, 4000);
			sp(npcId, 1294.3018f, 1730.9f, 316.875f, int8_t{0}, 4000);
			sp(npcId, 1292.1957f, 1726.921f, 316.875f, int8_t{0}, 4000);
			sp(npcId, 1293.0376f, 1722.3871f, 316.93515f, int8_t{0}, 4000);
			break;
		case StageType::START_STAGE_4_ROUND_1:
			switch (commons::utils::Rnd::get(1, 3)) {
				case 1:
					npcId = 217788;
					break;
				case 2:
					npcId = 217789;
					break;
				default:
					npcId = 217790;
					break;
			}
			sp(217786, 1263.4213f, 791.8533f, 436.64014f, int8_t{60}, 4000);
			sp(217786, 1267.2097f, 804.04456f, 436.64008f, int8_t{60}, 4000);
			sp(217786, 1267.0653f, 781.0253f, 436.64017f, int8_t{60}, 4000);
			sp(npcId, 1253.5984f, 791.6149f, 436.64008f, int8_t{0}, 4000);
			break;
		case StageType::START_ALTERNATIVE_STAGE_4_ROUND_1:
			switch (commons::utils::Rnd::get(1, 3)) {
				case 1:
					npcId = 217791; // http://www.aiondatabase.com/npc/217805 spawn 2 5%
					break;
				case 2:
					npcId = 217792;
					break;
				default:
					npcId = 217793;
					break;
			}
			sp(217787, 1252.525f, 248.50781f, 405.38016f, int8_t{60}, 4000);
			sp(217787, 1250.0901f, 237.69656f, 405.39676f, int8_t{60}, 4000);
			sp(217787, 1253.0117f, 225.77977f, 405.3801f, int8_t{60}, 4000);
			sp(npcId, 1242.3618f, 237.89081f, 405.3801f, int8_t{0}, 4000);
			break;
		case StageType::START_STAGE_5_ROUND_1:
			if (player != nullptr) {
				switch (player->getRace()) {
					case Race::ELYOS:
						switch (commons::utils::Rnd::get(1, 5)) {
							case 1:
								npcId = 217807;
								break;
							case 2:
								npcId = 217808;
								break;
							case 3:
								npcId = 217809;
								break;
							case 4:
								npcId = 217810;
								break;
							default:
								npcId = 217815;
								break;
						}
						break;
					case Race::ASMODIANS:
						switch (commons::utils::Rnd::get(1, 5)) {
							case 1:
								npcId = 217811;
								break;
							case 2:
								npcId = 217812;
								break;
							case 3:
								npcId = 217813;
								break;
							case 4:
								npcId = 217814;
								break;
							default:
								npcId = 217816;
								break;
						}
						break;
					default:
						break;
				}
				sp(npcId, 335.7365f, 337.93097f, 96.0909f, int8_t{0}, 4000);
			}
			break;
		case StageType::START_STAGE_6_ROUND_1:
			sp(217819, 1769.4579f, 1290.9342f, 389.11728f, int8_t{80}, 4000);
			break;
		default:
			break;
	}
}

// Java CrucibleChallengeInstance.java:580-585
void CrucibleChallengeInstance::sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, h] {
		if (!isInstanceDestroyed)
			spawn(npcId, x, y, z, h);
	}, time);
}

// Java CrucibleChallengeInstance.java:587-593
bool CrucibleChallengeInstance::changeStage(StageType type, int32_t time) {
	if (this->stageType.get() == type)
		return false;
	this->stageType = type;
	ThreadPoolManager::getInstance().schedule({this}, [this] { sendEventPacket(); }, time);
	return true;
}

// Java CrucibleChallengeInstance.java:595-602
void CrucibleChallengeInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 217758:
			spawn(205674, 345.52954f, 1662.6697f, 95.25f, int8_t{0});
			break;
	}
}

// Java CrucibleChallengeInstance.java:604-678 (Java's dropItems.add/clear on a null set is its NullPointerException)
void CrucibleChallengeInstance::onDropRegistered(Npc& npc, int32_t winnerObj) {
	services::drop::DropRegistrationService& drops = services::drop::DropRegistrationService::getInstance();
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems = drops.getCurrentDropMap().get(npc.getObjectId());
	auto requireDrops = [&dropItems]() -> runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& {
		if (dropItems == nullptr)
			throw runtime::NullPointerException("CrucibleChallengeInstance: no drop set");
		return *dropItems;
	};
	int32_t npcId = npc.getNpcId();
	int32_t itemId = 0;
	switch (npcId) {
		case 217758:
			requireDrops().add(drops.regDropItem(1, winnerObj, npcId, 186000134, 1));
			break;
		case 218571:
			requireDrops().clear();
			switch (commons::utils::Rnd::get(1, 3)) {
				case 1:
					itemId = 182006429;
					break;
				case 2:
					itemId = 182006430;
					break;
				case 3:
					itemId = 182006431;
					break;
			}
			requireDrops().add(drops.regDropItem(1, winnerObj, npcId, itemId, 1));
			break;
		case 218185:
			requireDrops().clear();
			requireDrops().add(drops.regDropItem(1, winnerObj, npcId, 188051349, 1));
			break;
		case 217827:
		case 217828:
		case 217829: {
			int32_t dropCount = 0; // Java: count
			switch (rewardCount) {
				case 0:
					switch (commons::utils::Rnd::get(1, 3)) {
						case 1:
							dropCount = 1;
							break;
						case 2:
							dropCount = 10;
							break;
					}
					break;
				case 1:
					switch (commons::utils::Rnd::get(1, 3)) {
						case 1:
							dropCount = 2;
							break;
						case 2:
							dropCount = 18;
							break;
					}
					break;
				case 2:
					switch (commons::utils::Rnd::get(1, 3)) {
						case 1:
							dropCount = 3;
							break;
						case 2:
							dropCount = 26;
							break;
						case 3:
							dropCount = 250;
							break;
					}
					break;
			}

			switch (npcId) {
				case 217827:
					deleteAliveNpcs({217828, 217829});
					break;
				case 217828:
					deleteAliveNpcs({217827, 217829});
					break;
				case 217829:
					deleteAliveNpcs({217827, 217828});
					break;
			}
			if (dropCount == 0) {
				if (rewardCount == 0) {
					sp(217837, npc.getX(), npc.getY(), npc.getZ(), int8_t{0}, 0);
				} else if (rewardCount == 1) {
					sp(217838, npc.getX(), npc.getY(), npc.getZ(), int8_t{0}, 0);
				}
				npc.getController().delete_();
				rewardCount++;
				return;
			}
			rewardCount++;
			if (npc.isInsideZone(ZoneName::get("ILLUSION_STADIUM_1_300320000"))) {
				sp(205667, 1258.8464f, 237.85518f, 405.39673f, int8_t{0}, 0);
			} else if (npc.isInsideZone(ZoneName::get("ILLUSION_STADIUM_6_300320000"))) {
				sp(205677, 1271.5472f, 791.36145f, 436.64017f, int8_t{0}, 0);
			}
			requireDrops().clear();
			requireDrops().add(drops.regDropItem(1, winnerObj, npcId, 186000130, dropCount));
			break;
		}
	}
}

} // namespace aion::gameserver::handlers::instance::crucible
