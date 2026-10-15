#include "aion/gameserver/handlers/instance/pvp/IdgelDomeInstance.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/CreatureTypeInfo.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance::pvp {

AION_INSTANCE_HANDLER(IdgelDomeInstance, 301310000);

// Java IdgelDomeInstance.java:33-35
IdgelDomeInstance::IdgelDomeInstance(WorldMapInstance& instance) : BasicPvpInstance(instance) {
}

runtime::Ref<IdgelDomeInstance> IdgelDomeInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<IdgelDomeInstance>(instance);
}

// Java IdgelDomeInstance.java:37-41
void IdgelDomeInstance::onInstanceCreate() {
	instanceScore = PvpInstanceScore::create(5600, 1120, 3360); // No info found for draws, so let's guess
	BasicPvpInstance::onInstanceCreate();
}

// Java IdgelDomeInstance.java:43-57
void IdgelDomeInstance::spawnFactionRelatedNpcs() {
	spawn((raceStartPosition == 0 ? 802384 : 802383), 254.9255f, 179.3691f, 80.3904f, int8_t{90}); // Quest Npcs
	spawn((raceStartPosition == 0 ? 802383 : 802384), 274.5722f, 338.7958f, 80.6454f, int8_t{31});
	spawnAndSetRespawn((raceStartPosition == 0 ? 702300 : 702301), 286.2041f, 338.7794f, 79.8274f, int8_t{105}, 20); // Defense Turrets
	spawnAndSetRespawn((raceStartPosition == 0 ? 702300 : 702301), 279.9443f, 332.1660f, 79.8274f, int8_t{105}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 702300 : 702301), 263.9707f, 341.6699f, 79.7848f, int8_t{15}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 702300 : 702301), 277.0629f, 355.2583f, 79.7844f, int8_t{75}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 702301 : 702300), 243.0305f, 179.2297f, 79.8241f, int8_t{45}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 702301 : 702300), 249.3263f, 185.8511f, 79.8241f, int8_t{45}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 702301 : 702300), 252.1385f, 162.8889f, 79.7711f, int8_t{15}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 702301 : 702300), 265.3730f, 176.5772f, 79.7719f, int8_t{75}, 20);
	spawnAndSetRespawn((raceStartPosition == 0 ? 802548 : 802549), 199.187f, 191.761f, 80.7466f, int8_t{15}, 180); // Lever
	spawnAndSetRespawn((raceStartPosition == 0 ? 802549 : 802548), 329.799f, 326.113f, 81.8731f, int8_t{75}, 180);
}

// Java IdgelDomeInstance.java:59-67
void IdgelDomeInstance::onStart() {
	updateProgress(InstanceProgressionType::START_PROGRESS);
	instance->forEachDoor([](model::gameobjects::StaticDoor& door) { door.setOpen(true); });
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(287249, 264.4382f, 258.58527f, 88.452042f, int8_t{31}); }, 600000));
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { onStop(false); }, 1200000));
	spawnChest();
	spawnChest();
}

// Java IdgelDomeInstance.java:69-95
void IdgelDomeInstance::setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) {
	int32_t scorePoints = instanceScore.get()->getPointsByRace(reward.getRace());
	if (reward.getRace() == winningRace) {
		reward.setBaseAp(instanceScore.get()->getWinnerApReward() + (isBossKilled ? 2000 : 0));
		reward.setBonusAp(2 * scorePoints / MAX_PLAYERS_PER_FACTION);
		reward.setBaseGp(50);
		reward.setReward1(186000242, 3, 0); // Custom: Ceramium Medal
		reward.setReward2(188053030, 1, 0); // Idgel Dome Reward Box
		if (isBossKilled) {
			int32_t mythicKunaxEqItemId = 0;
			if (commons::utils::Rnd::chance() < 20)
				mythicKunaxEqItemId = reward.getMythicKunaxEquipment(player);
			reward.setReward3(mythicKunaxEqItemId, mythicKunaxEqItemId == 0 ? 0 : 1);
			reward.setReward4(188053032, 1); // Idgel Dome Tribute Box
		}
	} else {
		reward.setBaseAp(instanceScore.get()->getLoserApReward());
		reward.setBonusAp(scorePoints / MAX_PLAYERS_PER_FACTION);
		reward.setBaseGp(10);
		reward.setReward1(186000242, 1, 0); // Custom: Ceramium Medal
		reward.setReward2(188053031, 1, 0); // Idgel Dome Consolation Prize
		if (winningRace == Race::NONE)
			reward.setBaseAp(instanceScore.get()->getDrawApReward()); // Base AP are overridden in a draw case
	}
	distributeRewards(player, reward);
}

// Java IdgelDomeInstance.java:97-109
void IdgelDomeInstance::onSpawn(VisibleObject& object) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<VisibleObject>(object))) {
		switch (npc->getNpcId()) {
			case 287249: { // Kunax
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF5_FORTRESS_RE_BOSS_SPAWN());
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_FORTRESS_RE_BOSSSPAWN());
				break;
			}
			case 802548:
			case 802549:
				for (const runtime::Ptr<Player>& p : instance->getPlayersInside())
					if (p->getRace() != npc->getRace())
						setFlameVentNoInteraction(*p, object);
				break;
		}
	}
}

// Java IdgelDomeInstance.java:111-113
void IdgelDomeInstance::setFlameVentNoInteraction(Player& player, VisibleObject& flameVent) {
	PacketSendUtility::sendPacket(player, SM_CUSTOM_SETTINGS(flameVent.getObjectId(), 0, model::getId(CreatureType::PEACE), 0));
}

// Java IdgelDomeInstance.java:115-136
void IdgelDomeInstance::onDie(Npc& npc) {
	BasicPvpInstance::onDie(npc);
	runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage(); // Checked with retail, maybe the player making the last hit gets the points
	if (player == nullptr || instanceScore.get()->getInstanceProgressionType() != InstanceProgressionType::START_PROGRESS)
		return;

	int32_t points = 0;
	bool isKunaxKilled = false;
	switch (npc.getNpcId()) {
		case 234186:
		case 234187:
		case 234189:
			points = 120;
			break;
		case 234751:
		case 234752:
		case 234753:
			points = 200;
			break;
		case 287249: {
			points = 6000;
			isKunaxKilled = true;
			break;
		}
	}
	if (points > 0)
		updatePoints(player, player->getRace(), npc.getObjectTemplate()->getL10n(), points);
	if (isKunaxKilled)
		onStop(true);
}

// Java IdgelDomeInstance.java:138-144
void IdgelDomeInstance::spawnChest() {
	std::vector<runtime::Ptr<WorldPosition>> positions = chestPositions.snapshot();
	const runtime::Ptr<WorldPosition>* picked = commons::utils::Rnd::get(positions);
	if (picked != nullptr) {
		runtime::Ref<WorldPosition> p = *picked;
		chestPositions.remove(p);
		spawn(702581 + commons::utils::Rnd::get(0, 2), p->getX(), p->getY(), p->getZ(), p->getHeading());
	}
}

// Java IdgelDomeInstance.java:146-148
void IdgelDomeInstance::scheduleChestRespawn() {
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] { spawnChest(); }, 120000));
}

// Java IdgelDomeInstance.java:150-160
void IdgelDomeInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 702581:
		case 702582:
		case 702583: {
			chestPositions.add(runtime::Ref<WorldPosition>(*npc.getPosition()));
			scheduleChestRespawn();
			break;
		}
		case 802548:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_FORTRESS_RE_FIRESPAWN_A());
			break;
		case 802549:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_FORTRESS_RE_FIRESPAWN_B());
			break;
	}
}

// Java IdgelDomeInstance.java:162-169
void IdgelDomeInstance::onEnterInstance(Player& player) {
	runtime::Ptr<Npc> forbiddenFlameVent = getNpc(player.getRace() == Race::ASMODIANS ? 802548 : 802549);
	if (forbiddenFlameVent != nullptr)
		setFlameVentNoInteraction(player, *forbiddenFlameVent);

	BasicPvpInstance::onEnterInstance(player);
}

// Java IdgelDomeInstance.java:171-177
void IdgelDomeInstance::portToStartPosition(Player& player) {
	if ((player.getRace() == Race::ELYOS && raceStartPosition == 0) || (player.getRace() == Race::ASMODIANS && raceStartPosition != 0))
		TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 269.76874f, 348.35953f, 79.44365f, int8_t{105});
	else
		TeleportService::teleportTo(player, instance->getMapId(), instance->getInstanceId(), 259.3971f, 169.18243f, 79.430855f, int8_t{45});
}

// Java IdgelDomeInstance.java:179-182
bool IdgelDomeInstance::isBoss(Npc& npc) {
	return npc.getNpcId() == 287249;
}

} // namespace aion::gameserver::handlers::instance::pvp
