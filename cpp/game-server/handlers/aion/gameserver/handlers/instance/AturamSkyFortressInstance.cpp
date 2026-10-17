#include "aion/gameserver/handlers/instance/AturamSkyFortressInstance.h"

#include <string>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(AturamSkyFortressInstance, 300240000);

// Java AturamSkyFortressInstance.java:40-42
AturamSkyFortressInstance::AturamSkyFortressInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<AturamSkyFortressInstance> AturamSkyFortressInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<AturamSkyFortressInstance>(instance);
}

// Java AturamSkyFortressInstance.java:44-141
void AturamSkyFortressInstance::onDie(Npc& npc) {
	if (isInstanceDestroyed.get()) {
		return;
	}

	switch (npc.getNpcId()) {
		case 702651:
			spawn(282281, 524.1896f, 489.7742f, 649.916f, int8_t{34});
			break;
		case 700982:
			spawn(282279, 467.7094f, 465.6622f, 647.93896f, int8_t{40});
			break;
		case 700983:
			spawn(282278, 449.5576f, 420.7812f, 652.9143f, int8_t{89});
			instance->setDoorState(68, true);
			break;
		case 702650:
			spawn(282277, 572.8088f, 459.4094f, 647.93896f, int8_t{15});
			break;
		case 702652:
			spawn(282280, 581.1f, 401.3544f, 648.6401f, int8_t{9});
			break;
		case 217373:
			spawn(730375, 374.85f, 424.32f, 653.52f, int8_t{0});
			break;
		case 701043:
			npc.getController().delete_();
			deleteAliveNpcs({701030});
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_00());
			break;
		case 217371:
			spawn(730374, npc.getX(), npc.getY(), npc.getZ(), int8_t{0});
			break;
		case 217370: {
			int32_t killed1 = officerKilled.incrementAndGet();
			if (killed1 == 4) {
				instance->setDoorState(174, true);
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_311());
				startOfficerWalkerEvent();
			} else if (killed1 == 8) {
				instance->setDoorState(175, true);
				startMarbataWalkerEvent();
			}
			npc.getController().delete_();
			break;
		}
		case 217656: {
			int32_t killed2 = chiefKilled.incrementAndGet();
			if (killed2 == 1) {
				startOfficerWalkerEvent();
			} else if (killed2 == 2) {
				instance->setDoorState(178, true);
				instance->setDoorState(308, false); // reopen side windows
				ThreadPoolManager::getInstance().schedule({this}, [this] { instance->setDoorState(307, true); }, 10000); // close side windows
			}
			npc.getController().delete_();
			break;
		}
		case 217382: {
			instance->setDoorState(307, false); // reopen side windows
			instance->setDoorState(230, true);
			runtime::Ptr<Player> player = npc.getAggroList().getMostPlayerDamage();
			if (player != nullptr) {
				AbyssPointsService::addAp(*player, 540);
			}
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_322());
			break;
		}
		case 218577:
			spawn(217382, 258.3894f, 796.7554f, 901.6453f, int8_t{80});
			break;
		case 701029: {
			runtime::Ptr<Npc> boss = instance->getNpc(217371);
			int32_t used = generators.incrementAndGet();
			if (boss != nullptr) {
				if (used == 1) {
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_01());
				} else if (used == 2) {
					boss->getEffectController()->removeEffect(19406);
					SkillEngine::getInstance().getSkill(*boss, 19407, 1, runtime::Ptr<VisibleObject>(boss))->useNoAnimationSkill();
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_02());
				} else if (used == 3) {
					boss->getEffectController()->removeEffect(19407);
					SkillEngine::getInstance().getSkill(*boss, 19408, 1, runtime::Ptr<VisibleObject>(boss))->useNoAnimationSkill();
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_03());
				} else if (used == 4) {
					boss->getEffectController()->removeEffect(19408);
					SkillEngine::getInstance().getSkill(*boss, 18117, 1, runtime::Ptr<VisibleObject>(boss))->useNoAnimationSkill();
					sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_04());
				}
			}
			npc.getController().delete_();
			break;
		}
		case 217369:
		case 217368:
		case 217655:
			npc.getController().delete_();
			break;
	}
}

// Java AturamSkyFortressInstance.java:143-148
void AturamSkyFortressInstance::startMarbataWalkerEvent() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_311());
	startWalk(*runtime::cast<Npc>(spawn(218577, 193.45583f, 802.1455f, 900.7575f, int8_t{103})), "3002400009");
	startWalk(*runtime::cast<Npc>(spawn(217655, 198.34431f, 801.4107f, 900.66125f, int8_t{110})), "30024000010");
	startWalk(*runtime::cast<Npc>(spawn(217655, 197.13315f, 798.7863f, 900.6499f, int8_t{110})), "30024000011");
}

// Java AturamSkyFortressInstance.java:150-159
void AturamSkyFortressInstance::startWalk(Npc& npc, std::string_view walkId) {
	runtime::Ref<Npc> walker(npc);
	ThreadPoolManager::getInstance().schedule({this}, [this, walker, id = std::string(walkId)] {
		if (!isInstanceDestroyed.get()) {
			walker->getSpawn()->setWalkerId(std::string_view(id));
			gameserver::ai::manager::WalkManager::startWalking(dynamic_cast<NpcAI&>(walker->getAi()));
			walker->setState(CreatureState::ACTIVE, true);
			PacketSendUtility::broadcastPacket(*walker,
				network::aion::serverpackets::SM_EMOTION(*walker, EmotionType::CHANGE_SPEED, 0, walker->getObjectId()));
		}
	}, 2000);
}

// Java AturamSkyFortressInstance.java:161-168
void AturamSkyFortressInstance::startOfficerWalkerEvent() {
	startWalk(*runtime::cast<Npc>(spawn(217655, 146.53816f, 713.5974f, 901.0108f, int8_t{111})), "3002400003");
	startWalk(*runtime::cast<Npc>(spawn(217655, 144.84991f, 720.9318f, 901.0604f, int8_t{96})), "3002400004");
	startWalk(*runtime::cast<Npc>(spawn(217655, 146.19899f, 709.60455f, 901.0078f, int8_t{110})), "3002400005");
	startWalk(*runtime::cast<Npc>(spawn(217656, 144.11845f, 716.8327f, 901.046f, int8_t{100})), "3002400006");
	startWalk(*runtime::cast<Npc>(spawn(217369, 144.96825f, 712.83344f, 901.0133f, int8_t{110})), "3002400007");
	startWalk(*runtime::cast<Npc>(spawn(217369, 144.75804f, 718.4293f, 901.05493f, int8_t{80})), "3002400008");
}

// Java AturamSkyFortressInstance.java:170-177
void AturamSkyFortressInstance::onInstanceCreate() {
	instance->setDoorState(177, true);
	runtime::Ptr<Npc> npc = instance->getNpc(217371);
	if (npc != nullptr) {
		SkillEngine::getInstance().getSkill(*npc, 19406, 1, runtime::Ptr<VisibleObject>(npc))->useNoAnimationSkill();
	}
}

// Java AturamSkyFortressInstance.java:179-182
void AturamSkyFortressInstance::onInstanceDestroy() {
	isInstanceDestroyed = true;
}

// Java AturamSkyFortressInstance.java:184-207
void AturamSkyFortressInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS::TYPE;
	using LOG = network::aion::serverpackets::SM_ATTACK_STATUS::LOG;
	switch (npc.getNpcId()) {
		case 730398:
			player->getLifeStats()->increaseHp(TYPE::HP, 5205, npc);
			player->getLifeStats()->increaseMp(TYPE::MP, 5205, 0, LOG::REGULAR);
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_Doping_02());
			npc.getController().delete_();
			break;
		case 730397:
			SkillEngine::getInstance().getSkill(npc, 19520, 51, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_Doping_01());
			break;
		case 730410:
			instance->setDoorState(90, true);
			break;
		case 731533:
			SkillEngine::getInstance().getSkill(*player, 21807, 1, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
			break;
		case 731534:
			SkillEngine::getInstance().getSkill(*player, 21808, 1, runtime::Ptr<VisibleObject>(player))->useNoAnimationSkill();
			break;
	}
}

// Java AturamSkyFortressInstance.java:209-217
void AturamSkyFortressInstance::onEnterZone(Player& player, ZoneInstance& zone) {
	if (zone.getAreaTemplate()->getZoneName() == ZoneName::get("SKY_FORTRESS_WAREHOUSE_ZONE_300240000")) {
		// wtf is that? Notify only one player ?
		if (msgIsSent.compareAndSet(false, true)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDStation_Doping_01_AD());
		}
	}
}

// Java AturamSkyFortressInstance.java:219-223
void AturamSkyFortressInstance::onPlayMovieEnd(Player& player, int32_t movieId) {
	if (movieId == 471)
		ThreadPoolManager::getInstance().schedule({this}, [this] { instance->setDoorState(308, true); }, 10000); // close side windows
}

} // namespace aion::gameserver::handlers::instance
