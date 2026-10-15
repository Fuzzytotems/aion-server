#include "aion/gameserver/handlers/instance/pvp/KamarBattlefieldInstance.h"

#include <cstdlib>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance::pvp {

AION_INSTANCE_HANDLER(KamarBattlefieldInstance, 301120000);

// Java KamarBattlefieldInstance.java:39-42 (the static block's generalsPos)
const std::vector<runtime::Ref<WorldPosition>>& KamarBattlefieldInstance::generalsPos() {
	static const auto* positions = new std::vector<runtime::Ref<WorldPosition>>{
		WorldPosition::create(301120000, 1437.7f, 1368.7f, 600.8967f, int8_t{40}),
		WorldPosition::create(301120000, 1172.2f, 1445, 586.55f, int8_t{35}),
		WorldPosition::create(301120000, 1428.67f, 1617.67f, 599.9493f, int8_t{70}),
	};
	return *positions;
}

// Java KamarBattlefieldInstance.java:39-52 (the static block's garnonPos)
const std::vector<runtime::Ref<WorldPosition>>& KamarBattlefieldInstance::garnonPos() {
	static const auto* positions = new std::vector<runtime::Ref<WorldPosition>>{
		WorldPosition::create(301120000, 1138.4039f, 1619.2574f, 598.43506f, int8_t{53}),
		WorldPosition::create(301120000, 1184.5309f, 1408.2471f, 586.6199f, int8_t{6}),
		WorldPosition::create(301120000, 1241.9187f, 1557.2854f, 585.2431f, int8_t{46}),
		WorldPosition::create(301120000, 1270.4377f, 1455.0625f, 595.2903f, int8_t{13}),
		WorldPosition::create(301120000, 1325.634f, 1326.134f, 596.4888f, int8_t{106}),
		WorldPosition::create(301120000, 1346.7902f, 1717.1029f, 598.43396f, int8_t{30}),
		WorldPosition::create(301120000, 1410.7446f, 1579.752f, 595.7288f, int8_t{93}),
		WorldPosition::create(301120000, 1455.881f, 1392.8229f, 598.5873f, int8_t{10}),
		WorldPosition::create(301120000, 1540.113f, 1395.6737f, 596.625f, int8_t{105}),
	};
	return *positions;
}

// Java KamarBattlefieldInstance.java:54-56
KamarBattlefieldInstance::KamarBattlefieldInstance(WorldMapInstance& instance) : BasicPvpInstance(instance) {
}

runtime::Ref<KamarBattlefieldInstance> KamarBattlefieldInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<KamarBattlefieldInstance>(instance);
}

// Java KamarBattlefieldInstance.java:58-63
void KamarBattlefieldInstance::onStart() {
	updateProgress(InstanceProgressionType::PREPARING);
	for (const runtime::Ptr<Player>& player : instance->getPlayersInside())
		portToStartPosition(*player); // split groups
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { endPreparingAndStart(); }, 60000));
}

// Java KamarBattlefieldInstance.java:65-73
void KamarBattlefieldInstance::endPreparingAndStart() {
	updateProgress(InstanceProgressionType::START_PROGRESS);
	openFirstDoors();

	const runtime::Ref<WorldPosition>& pos = *commons::utils::Rnd::get(garnonPos());
	spawn(801903, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading());
	tasks.add(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { onTimeProgressed(); }, 0, 60000));
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { onStop(false); }, 1800000));
}

// Java KamarBattlefieldInstance.java:75-151
void KamarBattlefieldInstance::onTimeProgressed() {
	timeInMin = static_cast<int8_t>(timeInMin.get() + 1);
	switch (timeInMin.get()) {
		case 5:
			spawn(802016, 1440.3145f, 1227.4073f, 587.36328f, int8_t{0}, 223);
			spawn(802017, 1109.5887f, 1532.7554f, 586.6358f, int8_t{0}, 221);
			spawn(802018, 1213.4902f, 1363.4617f, 613.93866f, int8_t{0}, 225);
			spawn(802019, 1527.215f, 1561.5153f, 613.47742f, int8_t{0}, 224);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_StartTeleporter_Spawn());
			break;
		case 10:
			spawn(801772, 1353.1956f, 1413.8037f, 598.75f, int8_t{0});
			spawn(801772, 1356.0574f, 1479.6165f, 594.15155f, int8_t{0});
			spawn(801772, 1371.584f, 1550.1755f, 595.375f, int8_t{0});
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_YunSupply_Spawn());
			break;
		case 12:
			spawnAndSetRespawn(701808, 1285.834f, 1489.1963f, 595.66486f, int8_t{0}, 180);
			spawnAndSetRespawn(701912, 1414.2816f, 1463.925f, 598.7676f, int8_t{0}, 180);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_SeigeWeapon_Spawn());
			break;
		case 14:
			spawn(801962, 1325.73f, 1521.42f, 700.0f, int8_t{15});
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_Dreadgion_Spawn());
			break;
		case 15:
			spawn(232847, 1221.6609f, 1563.3887f, 585.343f, int8_t{30});
			spawn(232847, 1312.6637f, 1426.5917f, 596.912f, int8_t{0});
			spawn(232847, 1421.0524f, 1503.8083f, 597.0f, int8_t{0});
			spawn(232847, 1347.8895f, 1278.5276f, 593.75f, int8_t{0});
			spawn(232848, 1318.0083f, 1423.2358f, 697.1422f, int8_t{0});
			spawn(232848, 1352.3656f, 1281.6598f, 593.75f, int8_t{0});
			spawn(232848, 1415.9098f, 1507.7222f, 597.0f, int8_t{0});
			spawn(232848, 1226.0847f, 1566.771f, 585.25f, int8_t{53});
			spawn(232849, 1328.4695f, 1667.7284f, 598.75f, int8_t{0});
			spawn(232849, 1316.2865f, 1526.8649f, 594.4299f, int8_t{100});
			spawn(232849, 1168.7726f, 1606.4891f, 598.7017f, int8_t{0});
			spawn(232850, 1134.1378f, 1498.5004f, 585.3203f, int8_t{15});
			spawn(232850, 1529.4595f, 1402.4359f, 597.5f, int8_t{20});
			spawn(232850, 1322.879f, 1531.0671f, 594.4299f, int8_t{100});
			spawn(232851, 1531.8644f, 1454.7493f, 596.7186f, int8_t{80});
			spawn(232851, 1321.8517f, 1525.4725f, 594.4299f, int8_t{100});
			spawn(232851, 1133.2808f, 1504.6725f, 585.22835f, int8_t{116});
			spawn(233261, 1357.5049f, 1434.2639f, 598.875f, int8_t{88});
			spawn(233261, 1375.0513f, 1531.0963f, 597.12115f, int8_t{16});
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_DrakanH_Spawn());
			break;
		case 18: {
			std::vector<runtime::Ref<WorldPosition>> temp(generalsPos());
			int32_t index = commons::utils::Rnd::nextInt(static_cast<int32_t>(temp.size()));
			runtime::Ref<WorldPosition> pos1 = temp[index];
			temp.erase(temp.begin() + index);
			spawn(232854, pos1->getX(), pos1->getY(), pos1->getZ(), pos1->getHeading());
			index = commons::utils::Rnd::nextInt(static_cast<int32_t>(temp.size()));
			pos1 = temp[index];
			temp.erase(temp.begin() + index);
			spawn(232853, pos1->getX(), pos1->getY(), pos1->getZ(), pos1->getHeading());
			index = commons::utils::Rnd::nextInt(static_cast<int32_t>(temp.size()));
			pos1 = temp[index];
			temp.erase(temp.begin() + index);
			spawn(232852, pos1->getX(), pos1->getY(), pos1->getZ(), pos1->getHeading());
			spawn(232846, 1442.18f, 1370.7f, 600.6902f, int8_t{40});
			spawn(232846, 1434.45f, 1365.7f, 600.70776f, int8_t{40});
			spawn(232846, 1178.58f, 1445.6f, 586.5563f, int8_t{35});
			spawn(232846, 1166.8f, 1442.0f, 586.5563f, int8_t{35});
			spawn(232846, 1427.12f, 1621.19f, 599.9493f, int8_t{70});
			spawn(232846, 1431.09f, 1613.77f, 599.9493f, int8_t{70});
			// spawn Bark
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_DrakanGeneral_Spawn());
			break;
		}
		case 25:
			spawn(232857, 1250.54f, 1646.07f, 584.9f, int8_t{100});
			spawn(232859, 1246.65f, 1645.06f, 584.9f, int8_t{100});
			spawn(232859, 1253.43f, 1649.13f, 584.9f, int8_t{100});
			spawn(232858, 1388.45f, 1438.7f, 600, int8_t{40});
			spawn(232860, 1394, 1440.34f, 600, int8_t{40});
			spawn(232860, 1385.74f, 1435.5f, 600, int8_t{40});
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDKamar_LightDarkGeneral_Spawn());
			break;
	}
}

// Java KamarBattlefieldInstance.java:153-171
void KamarBattlefieldInstance::setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) {
	int32_t scorePoints = instanceScore.get()->getPointsByRace(reward.getRace());
	if (reward.getRace() == winningRace) {
		reward.setBaseAp(instanceScore.get()->getWinnerApReward() + (isBossKilled ? 3850 : 0));
		reward.setBonusAp(2 * scorePoints / MAX_PLAYERS_PER_FACTION);
		reward.setBaseGp(100);
		reward.setReward1(188052670, 1, 0); // Kamar Victory Box
		if (isBossKilled && commons::utils::Rnd::chance() < 5)
			reward.setReward2(188950020, 1, 0); // CUSTOM: Special Courier Pass (Abyss Mythic/Lv. 61-65)
	} else {
		reward.setBaseAp(instanceScore.get()->getLoserApReward());
		reward.setBonusAp(scorePoints / MAX_PLAYERS_PER_FACTION);
		reward.setBaseGp(10);
		if (winningRace == Race::NONE)
			reward.setBaseAp(instanceScore.get()->getDrawApReward()); // Base AP are overridden in a draw case
	}
	distributeRewards(player, reward);
}

// Java KamarBattlefieldInstance.java:173-180
void KamarBattlefieldInstance::updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points) {
	BasicPvpInstance::updatePoints(player, race, npcL10n, points);

	int32_t diff = std::abs(instanceScore.get()->getAsmodiansPoints() - instanceScore.get()->getElyosPoints());
	if (diff >= 20000)
		onStop(false);
}

// Java KamarBattlefieldInstance.java:182-229
void KamarBattlefieldInstance::onDie(Npc& npc) {
	runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage();
	if (player == nullptr) {
		return;
	}
	int32_t points = 0;
	switch (npc.getNpcId()) {
		case 232856:
		case 232855:
		case 232852:
			points = 1250;
			break;
		case 701807:
		case 701808:
		case 701911:
		case 701912:
			points = 225;
			break;
		case 232847:
		case 232848:
		case 232849:
		case 232850:
		case 232851:
		case 233261:
			points = 140;
			break;
		case 233260:
		case 232841:
		case 232842:
		case 232843:
		case 232844:
		case 232845:
		case 232846:
			points = 50;
			break;
		case 801771:
			points = 75;
			break;
		case 232853:
			points = 3500;
			break;
	}
	if (points > 0) {
		updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), points);
	}
	// Correction (owner's decision 2026-10-15, both branches; docs/deviations/P6-instances.md): Java calls onStop(true) in the switch, before
	// updatePoints, which then finds the run ended and drops 232853's 3500 points. The points come first; the stop is skipped when they
	// already ended the run (a lead of 20,000), so the rewards are not given twice.
	if (npc.getNpcId() == 232853 && instanceScore.get()->getInstanceProgressionType() != InstanceProgressionType::END_PROGRESS)
		onStop(true);
}

// Java KamarBattlefieldInstance.java:231-265
void KamarBattlefieldInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	if (player == nullptr) {
		return;
	}
	int32_t points = 0;
	switch (npc.getNpcId()) {
		case 801903:
			points = 1500;
			break;
		case 801772:
			points = 525;
			break;
		case 801766:
		case 801767:
		case 801818:
		case 801819:
		case 801820:
		case 801821:
			points = 255;
			break;
		case 730861:
		case 730878:
		case 730879:
		case 730880:
			updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), 200);
			spawn(player->getRace() == Race::ELYOS ? 701900 : 701901, npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
			npc.getController().delete_();
			break;
	}
	if (points > 0) {
		updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), points);
		npc.getController().delete_();
	}
}

// Java KamarBattlefieldInstance.java:267-272
void KamarBattlefieldInstance::openFirstDoors() {
	instance->setDoorState(4, true);
	instance->setDoorState(8, true);
	instance->setDoorState(10, true);
	instance->setDoorState(11, true);
}

// Java KamarBattlefieldInstance.java:274-277
int32_t KamarBattlefieldInstance::getReinforceMemberPhaseDelay() {
	return 120000;
}

// Java KamarBattlefieldInstance.java:279-283
void KamarBattlefieldInstance::onInstanceCreate() {
	instanceScore = PvpInstanceScore::create(8750, 1750, 5250); // No info found for draws, so let's guess
	BasicPvpInstance::onInstanceCreate();
}

// Java KamarBattlefieldInstance.java:285-299
void KamarBattlefieldInstance::portToStartPosition(Player& player) {
	bool useAlternativePos = player.isInAlliance() && player.getPlayerAllianceGroup()->getObjectId() == 1001;
	if ((player.getRace() == Race::ELYOS && raceStartPosition == 0) || (player.getRace() == Race::ASMODIANS && raceStartPosition != 0)) {
		if (useAlternativePos)
			TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 1099.0986f, 1541.5055f, 585.0f);
		else
			TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 1535.6466f, 1573.8773f, 612.4217f);
	} else {
		if (useAlternativePos)
			TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 1446.6449f, 1232.9314f, 585.0623f);
		else
			TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 1204.9689f, 1350.8196f, 612.91205f);
	}
}

// Java KamarBattlefieldInstance.java:301-308
void KamarBattlefieldInstance::onEnterZone(Player& player, ZoneInstance& zone) {
	if (zone.getZoneTemplate()->getName() == ZoneName::get("LAMINA_301120000")) {
		instance->setDoorState(144, true); // crash airship
	} else if (zone.getZoneTemplate()->getName() == ZoneName::get("SPERO_301120000")) {
		instance->setDoorState(5, true); // crash airship
	}
}

} // namespace aion::gameserver::handlers::instance::pvp
