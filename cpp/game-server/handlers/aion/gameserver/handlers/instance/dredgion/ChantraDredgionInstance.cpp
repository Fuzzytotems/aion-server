#include "aion/gameserver/handlers/instance/dredgion/ChantraDredgionInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::dredgion {

AION_INSTANCE_HANDLER(ChantraDredgionInstance, 300210000);

// Java ChantraDredgionInstance.java:18-20
ChantraDredgionInstance::ChantraDredgionInstance(WorldMapInstance& instance) : DredgionInstance(instance) {
}

runtime::Ref<ChantraDredgionInstance> ChantraDredgionInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<ChantraDredgionInstance>(instance);
}

// Java ChantraDredgionInstance.java:23-53
void ChantraDredgionInstance::onEnterInstance(Player& player) {
	if (isInstanceStarted.get()->compareAndSet(false, true)) {
		sp(730311, 554.83081f, 173.87158f, 432.52448f, int8_t{0}, 9, 720000);
		sp(730312, 397.11661f, 184.29782f, 432.80328f, int8_t{0}, 42, 720000);
		if (commons::utils::Rnd::chance() < 21) {
			sp(216889, 484.1199f, 314.08817f, 403.7213f, int8_t{5}, 720000);
		}
		if (commons::utils::Rnd::chance() < 21) {
			sp(216890, 499.52f, 598.67f, 390.49f, int8_t{59}, 720000);
		}
		if (commons::utils::Rnd::chance() < 21) {
			spawn(216887, 486.26382f, 909.48175f, 405.24463f, int8_t{90});
		}
		if (commons::utils::Rnd::chance() < 51) {
			switch (commons::utils::Rnd::nextInt(2)) {
				case 0:
					spawn(216888, 416.3429f, 282.32785f, 409.7311f, int8_t{80});
					break;
				default:
					spawn(216888, 552.07446f, 289.058f, 409.7311f, int8_t{80});
					break;
			}
		}
		int32_t spawnTime = commons::utils::Rnd::get(10, 15) * 60 * 1000 + 120000;
		sendMsgByRace(1400633, Race::PC_ALL, spawnTime);
		sp(216941, 485.99f, 299.23f, 402.57f, int8_t{30}, spawnTime);
		startInstanceTask();
	}
	DredgionInstance::onEnterInstance(player);
}

// Java ChantraDredgionInstance.java:55-67
void ChantraDredgionInstance::onDieSurkana(Npc& npc, Player& mostPlayerDamage, int32_t points) {
	Race race = mostPlayerDamage.getRace();
	captureRoom(race, npc.getNpcId() + 14 - 700851);
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_ROOM_DESTROYED(utils::ChatUtil::l10n(model::getL10nId(race)), npc.getObjectTemplate()->getL10n()));
	getPlayerReward(mostPlayerDamage)->incrementCapturedZones();
	if (killedSurkanas.incrementAndGet() == 5) {
		spawn(216886, 485.33f, 832.26f, 416.64f, int8_t{55});
		sendMsgByRace(1400632, Race::PC_ALL, 0);
	}
	updateScore(mostPlayerDamage, runtime::Ptr<Creature>(npc), points, false);
	npc.getController().delete_();
}

// Java ChantraDredgionInstance.java:69-156
void ChantraDredgionInstance::onDie(Npc& npc) {
	if (instanceScore.get()->getInstanceProgressionType() != InstanceProgressionType::START_PROGRESS) {
		return;
	}
	switch (npc.getNpcId()) {
		case 730350: // Secondary Hatch teleporter
			sendMsgByRace(1400641, Race::PC_ALL, 0);
			spawn(730315, 415.07663f, 173.85265f, 432.53436f, int8_t{0}, 34);
			npc.getController().delete_();
			return;
		case 730349: // Escape Hatch teleporter
			sendMsgByRace(1400631, Race::PC_ALL, 0);
			spawn(730314, 396.979f, 184.392f, 433.940f, int8_t{0}, 42);
			npc.getController().delete_();
			return;
		case 730351:
			sendMsgByRace(1400226, Race::PC_ALL, 0);
			spawn(730345, 448.391998f, 493.641998f, 394.131989f, int8_t{90}, 12);
			npc.getController().delete_();
			return;
		case 730352:
			sendMsgByRace(1400227, Race::PC_ALL, 0);
			spawn(730346, 520.875977f, 493.401001f, 394.433014f, int8_t{90}, 133);
			npc.getController().delete_();
			return;
		case 216890:
		case 216889:
			return;
		default:
			break;
	}
	runtime::Ptr<Player> mostPlayerDamage = npc.getAggroList().getMostPlayerDamage();
	if (mostPlayerDamage == nullptr) {
		return;
	}
	Race race = mostPlayerDamage->getRace();
	switch (npc.getNpcId()) {
		case 700838:
		case 700839:
			onDieSurkana(npc, *mostPlayerDamage, 400);
			return;
		case 700840:
		case 700848:
		case 700849:
		case 700850:
		case 700851:
			onDieSurkana(npc, *mostPlayerDamage, 700);
			return;
		case 700845:
		case 700846:
			onDieSurkana(npc, *mostPlayerDamage, 800);
			return;
		case 700847:
			onDieSurkana(npc, *mostPlayerDamage, 900);
			return;
		case 700841:
		case 700842:
			onDieSurkana(npc, *mostPlayerDamage, 1000);
			return;
		case 700843:
		case 700844:
			onDieSurkana(npc, *mostPlayerDamage, 1100);
			return;
		case 216882: // Captain's Cabin teleport
			sendMsgByRace(1400652, Race::PC_ALL, 0);
			if (race == Race::ASMODIANS) {
				spawn(730358, 496.178f, 761.770f, 390.805f, int8_t{0}, 186);
			} else {
				spawn(730357, 473.759f, 761.864f, 390.805f, int8_t{0}, 33);
			}
			break;
		case 700836:
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 100, false);
			npc.getController().delete_();
			return;
		case 216886:
			if (!instanceScore.get()->isRewarded()) {
				updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 1000, false);
				stopInstance(instanceScore.get()->getRaceWithHighestPoints());
			}
			return;
		case 216941:
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 1000, false);
			return;
		case 216885:
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 500, false);
			return;
		default:
			break;
	}
	DredgionInstance::onDie(npc);
}

// Java ChantraDredgionInstance.java:159-162
void ChantraDredgionInstance::openFirstDoors() {
	instance->setDoorState(4, true);
	instance->setDoorState(173, true);
}

} // namespace aion::gameserver::handlers::instance::dredgion
