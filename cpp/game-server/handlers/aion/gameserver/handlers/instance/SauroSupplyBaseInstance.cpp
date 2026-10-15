#include "aion/gameserver/handlers/instance/SauroSupplyBaseInstance.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(SauroSupplyBaseInstance, 301130000);

/**
 * Java SauroSupplyBaseInstance.java:29-53, the static block: it runs during C++ static initialization (ZoneName.cpp's way: chestPoints, an
 * inline static member, is initialized before this definition; the adds run in a STARTUP TaskScope because the shim's operations need one).
 * Its static destruction releases the positions to the Reclaimer, whose state outlives static destruction (Reclaimer.cpp).
 */
const bool SauroSupplyBaseInstance::chestPointsFilled = [] {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	chestPoints.add(WorldPosition::create(301130000, 253.97533f, 363.97156f, 159.64023f, int8_t{0}));
	chestPoints.add(WorldPosition::create(301130000, 262.36151f, 393.78619f, 156.83209f, int8_t{30}));
	chestPoints.add(WorldPosition::create(301130000, 263.41193f, 463.63086f, 156.70573f, int8_t{90}));
	chestPoints.add(WorldPosition::create(301130000, 282.64951f, 330.15793f, 159.36792f, int8_t{0}));
	chestPoints.add(WorldPosition::create(301130000, 300.37936f, 332.13354f, 159.74071f, int8_t{60}));
	chestPoints.add(WorldPosition::create(301130000, 325.39862f, 364.31143f, 160.89003f, int8_t{60}));
	chestPoints.add(WorldPosition::create(301130000, 325.41577f, 388.60355f, 160.89003f, int8_t{60}));
	chestPoints.add(WorldPosition::create(301130000, 431.13638f, 481.81009f, 183.08244f, int8_t{0}));
	chestPoints.add(WorldPosition::create(301130000, 465.58185f, 412.19705f, 183.75404f, int8_t{90}));
	chestPoints.add(WorldPosition::create(301130000, 467.54926f, 377.6048f, 182.8183f, int8_t{30}));
	chestPoints.add(WorldPosition::create(301130000, 495.8497f, 359.10245f, 182.77852f, int8_t{30}));
	chestPoints.add(WorldPosition::create(301130000, 496.02997f, 381.77118f, 182.94859f, int8_t{30}));
	chestPoints.add(WorldPosition::create(301130000, 497.55228f, 402.35077f, 183.24211f, int8_t{30}));
	chestPoints.add(WorldPosition::create(301130000, 510.22513f, 535.37701f, 182.3832f, int8_t{90}));
	chestPoints.add(WorldPosition::create(301130000, 518.31726f, 463.9472f, 182.00262f, int8_t{45}));
	chestPoints.add(WorldPosition::create(301130000, 519.60052f, 505.98093f, 182.13989f, int8_t{60}));
	chestPoints.add(WorldPosition::create(301130000, 569.55652f, 495.64389f, 193.63138f, int8_t{0}));
	chestPoints.add(WorldPosition::create(301130000, 576.3725f, 500.66254f, 202.54437f, int8_t{105}));
	chestPoints.add(WorldPosition::create(301130000, 576.96289f, 471.08911f, 202.54466f, int8_t{15}));
	chestPoints.add(WorldPosition::create(301130000, 599.65344f, 361.96112f, 204.74123f, int8_t{0}));
	chestPoints.add(WorldPosition::create(301130000, 656.28381f, 398.28476f, 204.74123f, int8_t{90}));
	chestPoints.add(WorldPosition::create(301130000, 666.01855f, 363.71048f, 204.74123f, int8_t{90}));
	chestPoints.add(WorldPosition::create(301130000, 666.21979f, 345.6026f, 204.63773f, int8_t{30}));
	return true;
}();

// Java SauroSupplyBaseInstance.java:55-57
SauroSupplyBaseInstance::SauroSupplyBaseInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<SauroSupplyBaseInstance> SauroSupplyBaseInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<SauroSupplyBaseInstance>(instance);
}

// Java SauroSupplyBaseInstance.java:59-82
void SauroSupplyBaseInstance::onInstanceCreate() {
	// spawn Sauro Base Grave Robber (pool=1)
	switch (commons::utils::Rnd::get(1, 5)) {
		case 1:
			spawn(230846, 460.69705f, 390.10602f, 182.75943f, int8_t{0});
			break;
		case 2:
			spawn(230846, 463.92523f, 402.63937f, 183.31064f, int8_t{0});
			break;
		case 3:
			spawn(230846, 496.42526f, 412.04755f, 182.72771f, int8_t{0});
			break;
		case 4:
			spawn(230846, 497.27997f, 361.05948f, 182.45613f, int8_t{0});
			break;
		case 5:
			spawn(230846, 497.55908f, 389.86649f, 182.8175f, int8_t{0});
			break;
		default:
			break;
	}

	// spawn Commander Ranodim
	spawn(commons::utils::Rnd::get({230852, 233316, 233317}), 289.8795f, 343.75858f, 159.34445f, int8_t{90});

	std::vector<runtime::Ptr<WorldPosition>> temp = chestPoints.snapshot();
	for (int32_t i = 0; i < 8; i++) {
		int32_t index = commons::utils::Rnd::nextInt(static_cast<int32_t>(temp.size()));
		runtime::Ptr<WorldPosition> pos = temp[index];
		temp.erase(temp.begin() + index);
		spawn(230847, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading());
	}
	for (const runtime::Ptr<WorldPosition>& worldPosition : temp) {
		spawn(230848, worldPosition->getX(), worldPosition->getY(), worldPosition->getZ(), worldPosition->getHeading());
	}
}

// Java SauroSupplyBaseInstance.java:84-171
void SauroSupplyBaseInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 230837:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_03());
			instance->setDoorState(372, true);
			break;
		case 230849:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_01());
			instance->setDoorState(383, true);
			break;
		case 230850:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_04());
			instance->setDoorState(375, true);
			break;
		case 230851:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_02());
			instance->setDoorState(59, true);
			break;
		case 230852: // Ranodim
		case 233316:
		case 233317:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_06());
			instance->setDoorState(388, true);
			break;
		case 233255:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_05());
			instance->setDoorState(378, true);
			break;
		case 230838:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_07());
			instance->setDoorState(376, true);
			break;
		case 230853:
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_08());
			spawn(730872, 129.16417f, 432.32669f, 153.33147f, int8_t{2}, 3); // Boss Teleporter
			break;
		case 230854:
			spawn(801967, 91.69f, 889.8f, 411.45f, int8_t{120}); // Sauro Exit
			break;
		case 230855:
			spawn(801967, 289.38f, 889.82f, 411.45f, int8_t{120}); // Sauro Exit
			break;
		case 230856:
			spawn(801967, 485.1f, 889.93f, 411.45f, int8_t{0}); // Sauro Exit
			break;
		case 230857:
			spawn(801967, 721.84f, 889.93f, 411.45f, int8_t{60}); // Sauro Exit
			cancelTask();
			break;
		case 230858:
			spawn(801967, 880.82f, 889.93f, 411.45f, int8_t{120}); // Sauro Exit
			break;
		case 284437: // protective shield generator
		case 284445:
		case 284446:
			if (instance->getNpc(230857) != nullptr && !instance->getNpc(230857)->isDead()) {
				spawn(npc.getNpcId(), npc.getX(), npc.getY(), npc.getZ(), npc.getHeading());
				runtime::Ptr<Npc> ahuradim = instance->getNpc(230857);
				if (ahuradim != nullptr && !ahuradim->isDead()) {
					if (npc.getNpcId() == 284437) {
						startGeneratorTask(commons::utils::Rnd::nextBoolean() ? 284445 : 284446, *ahuradim);
					} else if (npc.getNpcId() == 284445) {
						startGeneratorTask(commons::utils::Rnd::nextBoolean() ? 284446 : 284437, *ahuradim);
					} else {
						startGeneratorTask(commons::utils::Rnd::nextBoolean() ? 284437 : 284445, *ahuradim);
					}
				}
			}
			npc.getController().delete_();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				runtime::Ptr<Npc> gen1 = instance->getNpc(284437);
				runtime::Ptr<Npc> gen2 = instance->getNpc(284445);
				runtime::Ptr<Npc> gen3 = instance->getNpc(284446);
				if (gen1 != nullptr) {
					SkillEngine::getInstance().getSkill(*gen1, 20773, 1, gen1)->useWithoutPropSkill();
				}
				if (gen2 != nullptr) {
					SkillEngine::getInstance().getSkill(*gen2, 20773, 1, gen2)->useWithoutPropSkill();
				}
				if (gen3 != nullptr) {
					SkillEngine::getInstance().getSkill(*gen3, 20773, 1, gen3)->useWithoutPropSkill();
				}
			}, 500);
			break;
		default:
			break;
	}
}

// Java SauroSupplyBaseInstance.java:173-189 (the lambdas keep Ahuradim and the generator alive, as Java's captures do)
void SauroSupplyBaseInstance::startGeneratorTask(int32_t npcId, Npc& ahuradim) {
	runtime::Ref<Npc> captain(ahuradim);
	scheduledGeneratorTask.set(ThreadPoolManager::getInstance().schedule({this}, [this, npcId, captain] {
		if (captain->isDead())
			return;
		runtime::Ptr<Npc> found = instance->getNpc(npcId);
		if (found == nullptr || found->isDead())
			return;
		runtime::Ref<Npc> generator(*found);
		generator->setTarget(captain);
		PacketSendUtility::broadcastMessage(generator, 1501014);
		scheduledGeneratorTask.set(ThreadPoolManager::getInstance().schedule({this}, [generator] {
			if (!generator->isDead()) {
				PacketSendUtility::broadcastMessage(generator, 1501015);
				SkillEngine::getInstance().getSkill(*generator, 21200, 1, generator)->useWithoutPropSkill();
			}
		}, 15 * 1000));
	}, 40 * 1000));
}

// Java SauroSupplyBaseInstance.java:191-195
void SauroSupplyBaseInstance::cancelTask() {
	runtime::FutureRef task = scheduledGeneratorTask.get();
	if (task != nullptr) {
		task->cancel(false);
	}
}

// Java SauroSupplyBaseInstance.java:197-202
void SauroSupplyBaseInstance::onBackHome(Npc& npc) {
	if (npc.getNpcId() == 230857) {
		cancelTask();
	}
}

// Java SauroSupplyBaseInstance.java:204-216
void SauroSupplyBaseInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	switch (npc.getNpcId()) {
		case 730876:
			TeleportService::teleportTo(*player, player->getWorldId(), player->getInstanceId(), 721.84f, 889.93f, 411.45f, int8_t{60},
				TeleportAnimation::FADE_OUT_BEAM);
			break;
		case 730877:
			TeleportService::teleportTo(*player, player->getWorldId(), player->getInstanceId(), 880.82f, 889.93f, 411.45f, int8_t{120},
				TeleportAnimation::FADE_OUT_BEAM);
			break;
		default:
			break;
	}
}

// Java SauroSupplyBaseInstance.java:218-224
bool SauroSupplyBaseInstance::isBoss(Npc& npc) {
	switch (npc.getNpcId()) {
		case 230849:
		case 230850:
		case 230851:
		case 230853:
		case 230857:
		case 230858:
		case 233255:
		case 233256:
		case 233257:
		case 233258:
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::handlers::instance
