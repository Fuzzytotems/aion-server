#include "aion/gameserver/handlers/instance/RaksangRuinsInstance.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(RaksangRuinsInstance, 300610000);

// Java RaksangRuinsInstance.java:31, 39-41
RaksangRuinsInstance::RaksangRuinsInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance), way(commons::utils::Rnd::get(0, 2)) {
}

runtime::Ref<RaksangRuinsInstance> RaksangRuinsInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<RaksangRuinsInstance>(instance);
}

// Java RaksangRuinsInstance.java:43-46
float RaksangRuinsInstance::getExpMultiplier() {
	return 1.8f;
}

// Java RaksangRuinsInstance.java:48-51
void RaksangRuinsInstance::onEnterInstance(Player& player) {
	spawn(206378 + way + (player.getRace() == Race::ASMODIANS ? 17 : 0), 818.103f, 931.0215f, 1207.4312f, int8_t{13});
}

// Java RaksangRuinsInstance.java:53-121 (Java's `case 0` without a break is the last arm)
void RaksangRuinsInstance::onDie(Npc& npc) {
	switch (npc.getNpcId()) {
		case 236010: // Trained Porgus
		case 236011: // Trained Worg
		case 236012: // Crumbling Skelesword
		case 236013: // Withering Husk
		case 236014: // Ragelich Adept
			if (waveKillz.incrementAndGet() >= 31) {
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_END());
				isDoorAccessible = true;
			}
			break;
		case 236074:
		case 236075:
		case 236076:
			switch (waveKillz.incrementAndGet()) {
				case 15:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_C_END());
					instance->setDoorState(457, true);
					break;
				case 30:
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_C_END());
					instance->setDoorState(64, true);
					break;
				default:
					break;
			}
			break;
		case 236020:
		case 236021:
		case 236096:
		case 236097:
			switch (waveKillz.incrementAndGet()) {
				case 28:
					spawnTask.get()->cancel(false);
					spawnTask.set(nullptr);
					spawns.set(0);
					instance->setDoorState(107, true);
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_B_END());
					break;
				case 56:
					spawn(236305, 331.107f, 787.326f, 147.798f, int8_t{47});
					break;
				default:
					break;
			}
			break;
		case 236084:
			instance->setDoorState(307, true);
			break;
		case 217469:
			instance->setDoorState(107, true);
			break;
		case 236303:
			instance->setDoorState(294, true);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_END());
			break;
		case 236304:
			instance->setDoorState(118, true);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_C_END());
			break;
		case 236305:
			instance->setDoorState(324, true);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_B_END());
			break;
		case 236306:
			spawn(730445, 620.65f, 663.44f, 522.049f, int8_t{27}); // Exit
			break;
		case 0:
			spawn(730438, 628.132f, 187.505f, 924.86f, int8_t{0});
			break;
		default:
			break;
	}
}

// Java RaksangRuinsInstance.java:123-147
void RaksangRuinsInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 702673: // Torment's Forge
		case 702674:
		case 702675:
		case 702690:
		case 702691:
		case 702692:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_START());
			delaySpawn(*player, 236074 + commons::utils::Rnd::nextInt(3), 2000);
			delaySpawn(*player, 236074 + commons::utils::Rnd::nextInt(3), 2000);
			delaySpawn(*player, 236074 + commons::utils::Rnd::nextInt(3), 8000);
			delaySpawn(*player, 236074 + commons::utils::Rnd::nextInt(3), 8000);
			delaySpawn(*player, 236074 + commons::utils::Rnd::nextInt(3), 12000);
			npc.getController().delete_();
			break;
		case 730438: // Terror' Vault
			if (isDoorAccessible.get())
				TeleportService::teleportTo(*player, *instance, 711.10895f, 312.82013f, 910.6781f);
			else
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_DOOR_CONDITION());
			break;
		default:
			break;
	}
}

// Java RaksangRuinsInstance.java:149-157
void RaksangRuinsInstance::delaySpawn(int32_t npcId, int32_t delay) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npcId] {
		if (!isInstanceDestroyed.get())
			spawn(npcId, 621.50f, 198.54f, 924.838f, int8_t{42});
	}, delay);
}

// Java RaksangRuinsInstance.java:159-259
void RaksangRuinsInstance::delaySpawn(int8_t step, int32_t delay) {
	spawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, step] {
		if (spawns.incrementAndGet() > 8 || isInstanceDestroyed.get()) {
			spawnTask.get()->cancel(false);
			return;
		}
		switch (step) {
			case 1:
				switch (spawns.get()) {
					case 1:
					case 3:
					case 5:
					case 7:
						spawn(236020, 344.016f, 630.834f, 146.514f, int8_t{0});
						spawn(236020, 325.803f, 645.552f, 146.514f, int8_t{0});
						spawn(236020, 307.303f, 614.676f, 146.514f, int8_t{0});
						spawn(236020, 329.768f, 608.466f, 146.514f, int8_t{0});
						break;
					case 2:
					case 4:
					case 6:
					case 8:
						spawn(236021, 312.577f, 605.469f, 146.514f, int8_t{0});
						spawn(236021, 307.422f, 630.051f, 146.514f, int8_t{0});
						spawn(236021, 337.200f, 639.658f, 146.514f, int8_t{0});
						break;
				}
				break;
			case 2:
				switch (spawns.get()) {
					case 1:
					case 3:
					case 5:
					case 7:
						spawn(236096, 303.755f, 772.090f, 148.988f, int8_t{0});
						spawn(236096, 350.070f, 801.111f, 145.082f, int8_t{0});
						spawn(236096, 332.003f, 767.004f, 147.394f, int8_t{0});
						spawn(236096, 325.803f, 810.720f, 144.885f, int8_t{0});
						break;
					case 2:
					case 4:
					case 6:
					case 8:
						spawn(236097, 344.371f, 781.986f, 147.159f, int8_t{0});
						spawn(236097, 339.273f, 813.446f, 145.390f, int8_t{0});
						spawn(236097, 317.126f, 766.019f, 148.844f, int8_t{0});
						break;
				}
				break;
			case 3:
				switch (spawns.get()) {
					case 1:
						delaySpawn(236010, 1000);
						delaySpawn(236010, 4000);
						delaySpawn(236010, 7000);
						delaySpawn(236010, 10000);
						delaySpawn(236010, 13000);
						break;
					case 2:
						delaySpawn(236010, 1000);
						delaySpawn(236010, 3000);
						delaySpawn(236010, 6000);
						delaySpawn(236011, 9000);
						delaySpawn(236011, 9500);
						break;
					case 3:
						delaySpawn(236011, 1000);
						delaySpawn(236011, 1500);
						delaySpawn(236011, 6000);
						delaySpawn(236011, 6500);
						delaySpawn(236011, 7000);
						break;
					case 4:
						delaySpawn(236011, 1000);
						delaySpawn(236011, 1500);
						delaySpawn(236013, 2000);
						break;
					case 5:
						delaySpawn(236010, 2000);
						delaySpawn(236010, 5000);
						delaySpawn(236010, 7000);
						delaySpawn(236011, 11000);
						delaySpawn(236011, 11500);
						break;
					case 6:
						delaySpawn(236011, 1000);
						delaySpawn(236011, 1500);
						delaySpawn(236013, 2000);
						delaySpawn(236011, 6000);
						delaySpawn(236011, 6500);
						break;
					case 7:
						delaySpawn(236012, 7000);
						delaySpawn(236012, 7500);
						delaySpawn(236014, 8000);
						break;
				}
				break;
		}
	}, 0, delay));
}

// Java RaksangRuinsInstance.java:261-266 (the task keeps the player alive, as Java's capture does)
void RaksangRuinsInstance::delaySpawn(Player& p, int32_t npcId, int32_t delay) {
	runtime::Ref<Player> player(p);
	ThreadPoolManager::getInstance().schedule({this}, [this, player, npcId] { rndSpawnInRange(*player, npcId, 5); }, delay);
}

// Java RaksangRuinsInstance.java:268-275 (Math.toRadians: angdeg / 180.0 * PI)
void RaksangRuinsInstance::rndSpawnInRange(Player& player, int32_t npcId, int32_t distance) {
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi;
	float x = player.getX() + static_cast<float>(std::cos(angleRadians) * distance);
	float y = player.getY() + static_cast<float>(std::sin(angleRadians) * distance);
	geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(player, x, y, player.getZ());
	int8_t headingTowardsPlayer = PositionUtil::getHeadingTowards(pos.getX(), pos.getY(), player.getX(), player.getY());
	spawn(npcId, pos.getX(), pos.getY(), pos.getZ(), headingTowardsPlayer);
}

// Java RaksangRuinsInstance.java:277-301
void RaksangRuinsInstance::onCreatureDetected(Npc& detector, Creature& detected) {
	if (runtime::as<Player>(runtime::Ptr<Creature>(detected)) != nullptr) {
		switch (detector.getNpcId()) {
			case 206197:
				detector.getController().delete_();
				instance->setDoorState(219, true); // collapse pathway
				break;
			case 206198:
				if (spawnTask.get() == nullptr && isEventStarted.compareAndSet(false, true)) {
					detector.getController().delete_();
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_B_START());
					delaySpawn(int8_t{1}, 10000);
				}
				break;
			case 206199:
				if (spawnTask.get() == nullptr && isEventStarted.compareAndSet(true, false)) {
					detector.getController().delete_();
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_B_START());
					delaySpawn(int8_t{2}, 10000);
				}
				break;
			default:
				break;
		}
	}
}

// Java RaksangRuinsInstance.java:303-311
void RaksangRuinsInstance::onEnterZone(Player& player, ZoneInstance& zone) {
	if (zone.getZoneTemplate()->getName() == ZoneName::get("IDRAKSHA_SOLO_WAVE_01_206399_1_300610000")) {
		if (spawnTask.get() == nullptr && isEventStarted.compareAndSet(false, true)) {
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_START());
			delaySpawn(int8_t{3}, 20000);
		}
	}
}

// Java RaksangRuinsInstance.java:313-318
void RaksangRuinsInstance::onInstanceDestroy() {
	isInstanceDestroyed = true;
	runtime::FutureRef task = spawnTask.get();
	if (task != nullptr)
		task->cancel(true);
}

} // namespace aion::gameserver::handlers::instance
