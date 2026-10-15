#include "aion/gameserver/handlers/instance/dredgion/BaranathDredgionInstance.h"

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

AION_INSTANCE_HANDLER(BaranathDredgionInstance, 300110000);

// Java BaranathDredgionInstance.java:18-20
BaranathDredgionInstance::BaranathDredgionInstance(WorldMapInstance& instance) : DredgionInstance(instance) {
}

runtime::Ref<BaranathDredgionInstance> BaranathDredgionInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<BaranathDredgionInstance>(instance);
}

// Java BaranathDredgionInstance.java:23-34
void BaranathDredgionInstance::onEnterInstance(Player& player) {
	if (isInstanceStarted.get()->compareAndSet(false, true)) {
		if (commons::utils::Rnd::chance() < 51) {
			if (commons::utils::Rnd::nextBoolean())
				spawn(215093, 416.3429f, 282.32785f, 409.7311f, int8_t{80});
			else
				spawn(215093, 552.07446f, 289.058f, 409.7311f, int8_t{80});
		}
		startInstanceTask();
	}
	DredgionInstance::onEnterInstance(player);
}

// Java BaranathDredgionInstance.java:36-48
void BaranathDredgionInstance::onDieSurkana(Npc& npc, Player& mostPlayerDamage, int32_t points) {
	Race race = mostPlayerDamage.getRace();
	captureRoom(race, npc.getNpcId() + 14 - 700498);
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_ROOM_DESTROYED(utils::ChatUtil::l10n(model::getL10nId(race)), npc.getObjectTemplate()->getL10n()));
	if (killedSurkanas.incrementAndGet() == 5) {
		spawn(214823, 485.423f, 808.826f, 416.868f, int8_t{30});
		sendMsgByRace(1401416, Race::PC_ALL, 0);
	}
	getPlayerReward(mostPlayerDamage)->incrementCapturedZones();
	updateScore(mostPlayerDamage, runtime::Ptr<Creature>(npc), points, false);
	npc.getController().delete_();
}

// Java BaranathDredgionInstance.java:50-117
void BaranathDredgionInstance::onDie(Npc& npc) {
	runtime::Ptr<Player> mostPlayerDamage = npc.getAggroList().getMostPlayerDamage();
	if (mostPlayerDamage == nullptr || instanceScore.get()->getInstanceProgressionType() != InstanceProgressionType::START_PROGRESS) {
		return;
	}
	Race race = mostPlayerDamage->getRace();
	switch (npc.getNpcId()) {
		case 700485:
		case 700486:
		case 700495:
		case 700496:
			onDieSurkana(npc, *mostPlayerDamage, 500);
			return;
		case 700497:
		case 700498:
			onDieSurkana(npc, *mostPlayerDamage, 700);
			return;
		case 700493:
		case 700492:
			onDieSurkana(npc, *mostPlayerDamage, 800);
			return;
		case 700487:
			onDieSurkana(npc, *mostPlayerDamage, 900);
			return;
		case 700490:
		case 700491:
			onDieSurkana(npc, *mostPlayerDamage, 600);
			return;
		case 700488:
		case 700489:
			onDieSurkana(npc, *mostPlayerDamage, 1080);
			return;
		case 214823:
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 1000, false);
			stopInstance(race);
			if (race == Race::ELYOS) {
				sendMsgByRace(1400230, Race::ELYOS, 0);
			} else {
				sendMsgByRace(1400231, Race::ASMODIANS, 0);
			}
			return;
		case 700508:
			switch (race) {
				case Race::ELYOS:
					spawn(700502, 520.88f, 493.40f, 395.34f, int8_t{28}, 16);
					sendMsgByRace(1400226, Race::ELYOS, 0);
					break;
				case Race::ASMODIANS:
					spawn(700502, 448.39f, 493.64f, 395.04f, int8_t{108}, 12);
					sendMsgByRace(1400227, Race::ASMODIANS, 0);
					break;
				default:
					break;
			}
			return;
		case 700506:
			switch (race) {
				case Race::ELYOS:
					spawn(730214, 567.59f, 175.20f, 432.28f, int8_t{33});
					sendMsgByRace(1400228, Race::ELYOS, 0);
					break;
				case Race::ASMODIANS:
					spawn(730214, 402.33f, 175.12f, 432.28f, int8_t{41});
					sendMsgByRace(1400229, Race::ASMODIANS, 0);
					break;
				default:
					break;
			}
			return;
		default:
			break;
	}
	DredgionInstance::onDie(npc);
}

// Java BaranathDredgionInstance.java:120-122
void BaranathDredgionInstance::onInstanceCreate() {
	initializeInstance(3000, 1500, 2250);
}

// Java BaranathDredgionInstance.java:125-128
void BaranathDredgionInstance::openFirstDoors() {
	instance->setDoorState(17, true);
	instance->setDoorState(18, true);
}

} // namespace aion::gameserver::handlers::instance::dredgion
