#include "aion/gameserver/handlers/instance/abyss/TheHexwayInstance.h"

#include <format>
#include <optional>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(TheHexwayInstance, 300700000);

// Java TheHexwayInstance.java:64-66
TheHexwayInstance::TheHexwayInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<TheHexwayInstance> TheHexwayInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<TheHexwayInstance>(instance);
}

// Java TheHexwayInstance.java:69-71
void TheHexwayInstance::onInstanceCreate() {
	initTimerTrigger();
}

// Java TheHexwayInstance.java:74-116
bool TheHexwayInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "HEXWAY_BONUSCHEST") {
		if (bonusChestStartMillis.compareAndSet(0, commons::utils::currentTimeMillis())) {
			// schedule bonus chest spawn condition change
			disableBonusChestSpawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (bonusChestSpawnAllowed.compareAndSet(true, false))
					broadcastYellowMessage(MSG_TIMER_EXPIRED);
			}, bonusChestTimeLimitSeconds * 1000));
			// send info message to all players
			broadcastYellowMessage(MSG_TIMER_STARTED);
			// schedule timer updates
			for (int32_t n = 0; n < notifyTimesSeconds->length(); n++) {
				const int32_t notifyTimeSecond = notifyTimesSeconds->get(n);
				int32_t scheduleDelaySeconds = bonusChestTimeLimitSeconds - notifyTimeSecond;
				if (scheduleDelaySeconds > 0)
					timerProgressMsgTasks.add(ThreadPoolManager::getInstance().schedule({this},
						[this, notifyTimeSecond] { sendTimeStringToPlayers(notifyTimeSecond); }, scheduleDelaySeconds * int64_t{1000}));
			}
		}
	} else if (flyingRing.starts_with("HEXWAY_BOSS_")) {
		int32_t bossIndex = std::stoi(std::string(flyingRing.substr(12))); // Java: Integer.parseInt
		if (bossIndex >= 0 && bossIndex <= 5) {
			if (stageStartMillis.compareAndSet(bossIndex, 0, commons::utils::currentTimeMillis())) {
				attackedBossCount.incrementAndGet();
				scheduleBossDespawn(bossIndex, bossTimeLimitsSeconds->get(bossIndex) * 1000);
			}
			int64_t stageStartTimeMillis = stageStartMillis.get(bossIndex);
			if (stageStartTimeMillis > 0) {
				int32_t bossTimeLimitSeconds = bossTimeLimitsSeconds->get(bossIndex);
				int32_t elapsedTimeMillis = static_cast<int32_t>(commons::utils::currentTimeMillis() - stageStartTimeMillis);
				if (elapsedTimeMillis <= bossTimeLimitSeconds * 1000 && scheduledBossDespawnTasks->get(bossIndex) != nullptr) {
					// add player to stage mapping
					playerStageMapping.put(player.getObjectId(), bossIndex);
					// send quest timer to player
					int32_t remainingTimeSeconds = bossTimeLimitSeconds - elapsedTimeMillis / 1000;
					PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, remainingTimeSeconds));
				}
			}
		}
	}
	return false;
}

// Java TheHexwayInstance.java:119-123
void TheHexwayInstance::onLeaveInstance(Player& player) {
	GeneralInstanceHandler::onLeaveInstance(player);
	playerStageMapping.remove(player.getObjectId());
	PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 0));
}

// Java TheHexwayInstance.java:126-135
void TheHexwayInstance::onInstanceDestroy() {
	// cleanup all scheduled tasks
	for (int32_t i = 0; i < scheduledBossDespawnTasks->length(); i++) {
		runtime::FutureRef scheduledBossDespawn = scheduledBossDespawnTasks->get(i);
		if (scheduledBossDespawn != nullptr && !scheduledBossDespawn->isDone())
			scheduledBossDespawn->cancel(true);
	}
	runtime::FutureRef disableTask = disableBonusChestSpawnTask.get();
	if (disableTask != nullptr && !disableTask->isDone())
		disableTask->cancel(true);
	cancelTimeInformTasks();
}

// Java TheHexwayInstance.java:138-211
void TheHexwayInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		// manager jarka
		case 219609:
			// open barricades
			deleteAliveNpcs({219617});
			break;
		case 219611:
		case 286933:
		case 219612:
		case 219613:
		case 219610:
		case 219614:
			// bridge bosses
			for (int32_t bossIndex = 0; bossIndex < bossNpcIds->length(); bossIndex++) {
				int32_t bossNpcId = bossNpcIds->get(bossIndex);
				if (npc.getNpcId() == bossNpcId) {
					cancelBossDespawn(bossIndex);
					spawnBossChest(bossIndex);
					instance->setDoorState(treasureDoorIds->get(bossIndex), true);
					// remove quest timers for affected players
					for (const int32_t playerObjId : playerStageMapping.keySet()) {
						std::optional<int32_t> stage = playerStageMapping.get(playerObjId);
						if (!stage) // Java: the unboxing of a mapping removed meanwhile
							throw runtime::NullPointerException("playerStageMapping.get(" + std::to_string(playerObjId) + ")");
						if (*stage == bossIndex) {
							playerStageMapping.remove(playerObjId);
							runtime::Ptr<Player> player = instance->getPlayer(playerObjId);
							if (player != nullptr)
								PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(0, 0));
						}
					}
					// spawn bonus loot chest if all 6 boss npc are killed in the given time limits
					if (killedBossCount.incrementAndGet() == 6 && attackedBossCount.get() == 6) {
						if (bonusChestSpawnAllowed.compareAndSet(true, false)) {
							runtime::FutureRef disableTask = disableBonusChestSpawnTask.get();
							if (disableTask != nullptr && !disableTask->isDone())
								disableTask->cancel(true);
							spawnBonusChest();
						}
						cancelTimeInformTasks();
					}
					int32_t remainingTimeSeconds = static_cast<int32_t>(
						(bonusChestStartMillis.get() + bonusChestTimeLimitSeconds * 1000 - commons::utils::currentTimeMillis()) / 1000);
					if (remainingTimeSeconds > 0)
						sendTimeStringToPlayers(remainingTimeSeconds);
					break;
				}
			}
			break;
		// handle 2nd bridge boss
		case 219635:
		case 219636:
			if (handlingSecondBoss.compareAndSet(false, true)) {
				int32_t secondNpcId = npc.getNpcId() == 219635 ? 219636 : 219635;
				runtime::Ptr<Npc> secondNpc = getNpc(secondNpcId);
				if (secondNpc != nullptr) {
					int32_t currentHpPercentage = secondNpc->getLifeStats()->getHpPercentage();
					int32_t bossId = bossNpcIds->get(1);
					runtime::Ptr<Npc> bossNpc = getNpc(bossId);
					if (bossNpc != nullptr) {
						int32_t percentageToDrop = 25 - currentHpPercentage;
						if (percentageToDrop > 0) {
							int32_t dmgToApply = static_cast<int32_t>(bossNpc->getLifeStats()->getMaxHp() * (percentageToDrop * 0.8 / 100));
							runtime::Ref<Npc> boss(*bossNpc);
							ThreadPoolManager::getInstance().schedule({this}, [boss, dmgToApply] {
								if (boss->isDead() || boss->getLifeStats()->isAboutToDie())
									return;
								boss->getController().onAttack(*boss, dmgToApply, std::nullopt);
							}, 1000);
						}
						secondNpc->getController().delete_();
					}
				}
				handlingSecondBoss.set(false);
			}
			break;
		default:
			break;
	}
}

// Java TheHexwayInstance.java:214-220
bool TheHexwayInstance::onReviveEvent(Player& player) {
	PlayerReviveService::revive(player, 25, 25, false, 0);
	player.getGameStats()->updateStatsAndSpeedVisually();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	TeleportService::teleportTo(player, mapId, 672.8343f, 606.2250f, 321.1900f, int8_t{0});
	return true;
}

// Java TheHexwayInstance.java:223-226
void TheHexwayInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	if (npc.getNpcId() == 700455)
		TeleportService::moveToInstanceExit(*player, mapId, player->getRace());
}

// Java TheHexwayInstance.java:228-233
void TheHexwayInstance::cancelBossDespawn(int32_t bossIndex) {
	runtime::FutureRef scheduledDespawn = scheduledBossDespawnTasks->get(bossIndex);
	if (scheduledDespawn != nullptr && !scheduledDespawn->isDone())
		scheduledDespawn->cancel(true);
	(*scheduledBossDespawnTasks)[bossIndex] = nullptr;
}

// Java TheHexwayInstance.java:235-243
void TheHexwayInstance::scheduleBossDespawn(int32_t bossIndex, int32_t despawnDelayMillis) {
	runtime::FutureRef despawnTask = ThreadPoolManager::getInstance().schedule({this}, [this, bossIndex] {
		bool chestSpawnWasAllowed = bonusChestSpawnAllowed.getAndSet(false);
		deleteAliveNpcs({bossNpcIds->get(bossIndex)});
		cancelTimeInformTasks();
		broadcastYellowMessage(chestSpawnWasAllowed ? MSG_BOSS_FAILED_NO_BOX : MSG_BOSS_FAILED);
	}, despawnDelayMillis);
	(*scheduledBossDespawnTasks)[bossIndex] = despawnTask;
}

// Java TheHexwayInstance.java:245-256
void TheHexwayInstance::cancelTimeInformTasks() {
	if (timerProgressMsgTasks.isEmpty())
		return;
	auto taskIterator = timerProgressMsgTasks.iterator();
	while (taskIterator.hasNext()) {
		runtime::FutureRef timerProgressMsgTask = taskIterator.next();
		if (timerProgressMsgTask != nullptr && !timerProgressMsgTask->isDone()) {
			timerProgressMsgTask->cancel(true);
			taskIterator.remove();
		}
	}
}

// Java TheHexwayInstance.java:258-261
void TheHexwayInstance::spawnBonusChest() {
	broadcastYellowMessage(MSG_SUCCESSFUL);
	spawn(701664, 485.59f, 585.42f, 357.0f, int8_t{0});
}

// Java TheHexwayInstance.java:263-290
void TheHexwayInstance::spawnBossChest(int32_t bossIndex) {
	switch (bossIndex) {
		case 0:
			spawn(701663, 223.6263f, 427.2576f, 364.6045f, int8_t{117});
			spawn(701662, 223.5121f, 409.0381f, 365.0105f, int8_t{25});
			break;
		case 1:
			spawn(701663, 195.7458f, 494.1713f, 365.0105f, int8_t{2});
			spawn(701662, 193.7205f, 499.2636f, 365.0105f, int8_t{103});
			break;
		case 2:
			spawn(701663, 197.9107f, 566.2823f, 365.0105f, int8_t{91});
			spawn(701662, 181.2328f, 537.9518f, 365.0105f, int8_t{17});
			break;
		case 3:
			spawn(701663, 185.4841f, 630.1168f, 365.0105f, int8_t{108});
			spawn(701662, 201.0349f, 612.7051f, 364.6045f, int8_t{25});
			break;
		case 4:
			spawn(701663, 192.4604f, 673.4794f, 365.0105f, int8_t{13});
			spawn(701662, 213.7684f, 696.3926f, 365.0105f, int8_t{25});
			break;
		case 5:
			spawn(701663, 241.1405f, 754.4207f, 365.0105f, int8_t{96});
			spawn(701662, 214.3266f, 743.3648f, 365.0105f, int8_t{25});
			break;
		default:
			break;
	}
}

// Java TheHexwayInstance.java:292-306
void TheHexwayInstance::initTimerTrigger() {
	spawnBonusChestTimerTrigger();
	spawnBossTimerTrigger(0, *Point3D::create(321.3199, 458.7944, 368.2764), *Point3D::create(317.8786, 465.5034, 361.7755),
		*Point3D::create(314.5425, 472.0469, 361.7755));
	spawnBossTimerTrigger(1, *Point3D::create(302.9791, 504.9413, 368.2764), *Point3D::create(300.9405, 511.9926, 361.7755),
		*Point3D::create(298.7013, 519.5820, 361.7755));
	spawnBossTimerTrigger(2, *Point3D::create(293.5730, 553.3477, 368.2764), *Point3D::create(292.3480, 560.7227, 361.7755),
		*Point3D::create(291.3300, 568.1430, 361.7755));
	spawnBossTimerTrigger(3, *Point3D::create(292.1579, 602.8574, 368.2764), *Point3D::create(292.4263, 610.3403, 361.7755),
		*Point3D::create(292.6229, 617.7412, 361.7755));
	spawnBossTimerTrigger(4, *Point3D::create(299.2268, 651.8597, 368.2764), *Point3D::create(300.9197, 659.2391, 361.7755),
		*Point3D::create(302.4053, 666.5773, 361.7755));
	spawnBossTimerTrigger(5, *Point3D::create(315.2073, 698.8103, 368.2764), *Point3D::create(317.9026, 705.6396, 361.7755),
		*Point3D::create(320.6522, 712.8006, 361.7755));
}

// Java TheHexwayInstance.java:308-312 (the rings' templates are immortal, as static data: FlyRingTemplate.h)
void TheHexwayInstance::spawnBonusChestTimerTrigger() {
	runtime::Ref<Point3D> center = Point3D::create(576.2102, 585.4146, 353.90677);
	runtime::Ref<Point3D> p1 = Point3D::create(576.2102, 585.4146, 359.90677);
	runtime::Ref<Point3D> p2 = Point3D::create(575.18384, 596.36664, 353.90677);
	runtime::Ref<FlyRing> f1 = VisibleObject::create<FlyRing>(new FlyRingTemplate("HEXWAY_BONUSCHEST", mapId, *center, *p1, *p2, 10),
		instance->getInstanceId());
	f1->spawn();
}

// Java TheHexwayInstance.java:314-317
void TheHexwayInstance::spawnBossTimerTrigger(int32_t number, Point3D& p1, Point3D& center, Point3D& p2) {
	runtime::Ref<FlyRing> timerBarrier = VisibleObject::create<FlyRing>(
		new FlyRingTemplate("HEXWAY_BOSS_" + std::to_string(number), mapId, center, p1, p2, 10), instance->getInstanceId());
	timerBarrier->spawn();
}

// Java TheHexwayInstance.java:319-321
void TheHexwayInstance::broadcastYellowMessage(std::string_view message) {
	PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "", message, ChatType::BRIGHT_YELLOW_CENTER));
}

// Java TheHexwayInstance.java:323-326
void TheHexwayInstance::sendTimeStringToPlayers(int32_t leftTimeSeconds) {
	if (bonusChestSpawnAllowed.get())
		broadcastYellowMessage(javaFormat(MSG_TIMER_NOTIFY, secondsToReadableString(leftTimeSeconds)));
}

/** Java String.format of a pattern with one %s */
std::string TheHexwayInstance::javaFormat(std::string_view pattern, std::string_view argument) {
	std::string result(pattern);
	result.replace(result.find("%s"), 2, argument);
	return result;
}

// Java TheHexwayInstance.java:328-340
std::string TheHexwayInstance::secondsToReadableString(int32_t remainingTimeSeconds) {
	int32_t remainingMinutes = remainingTimeSeconds / 60;
	int32_t remainingSeconds = remainingTimeSeconds - remainingMinutes * 60;
	std::string msg;
	if (remainingMinutes == 0)
		msg = std::format("{}s", remainingSeconds);
	else if (remainingSeconds == 0)
		msg = std::format("{}m", remainingMinutes);
	else
		msg = std::format("{}m {}s", remainingMinutes, remainingSeconds);
	return msg;
}

} // namespace aion::gameserver::handlers::instance::abyss
