#include "aion/gameserver/handlers/instance/dredgion/TerathDredgionInstance.h"

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

AION_INSTANCE_HANDLER(TerathDredgionInstance, 300440000);

// Java TerathDredgionInstance.java:20-22
TerathDredgionInstance::TerathDredgionInstance(WorldMapInstance& instance) : DredgionInstance(instance) {
}

runtime::Ref<TerathDredgionInstance> TerathDredgionInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<TerathDredgionInstance>(instance);
}

// Java TerathDredgionInstance.java:25-55
void TerathDredgionInstance::onEnterInstance(Player& player) {
	if (isInstanceStarted.get()->compareAndSet(false, true)) {
		sp(730558, 415.034f, 174.004f, 433.940f, int8_t{0}, 34, 720000);
		sp(730559, 572.038f, 185.252f, 433.940f, int8_t{0}, 10, 720000);
		sendMsgByRace(1401424, Race::PC_ALL, 720000);
		if (commons::utils::Rnd::chance() < 21) {
			sp(233372, 476.63f, 312.16f, 402.89807f, int8_t{97}, 720000, "5540A84BAD08498B96C315281F6418D0BD825175");
		}
		if (commons::utils::Rnd::chance() < 21) {
			sp(233373, 485.403f, 596.602f, 390.944f, int8_t{90}, 720000);
		}
		if (commons::utils::Rnd::chance() < 21) {
			sp(233379, 486.26382f, 906.011f, 405.24463f, int8_t{90}, 720000);
		}
		if (commons::utils::Rnd::chance() < 51) {
			switch (commons::utils::Rnd::nextInt(2)) { // Supervisor Chitan
				case 0:
					spawn(233362, 421.89111f, 285.20471f, 409.7311f, int8_t{80});
					break;
				default:
					spawn(233362, 551.407f, 289.058f, 409.7311f, int8_t{80});
					break;
			}
		}
		int32_t spawnTime = commons::utils::Rnd::get(10, 15) * 60 * 1000 + 120000;
		sendMsgByRace(1401417, Race::PC_ALL, spawnTime);
		sp(233377, 484.664f, 314.207f, 403.715f, int8_t{30}, spawnTime); // Enforcer Udara
		startInstanceTask();
	}
	DredgionInstance::onEnterInstance(player);
}

// Java TerathDredgionInstance.java:57-71
void TerathDredgionInstance::onDieSurkana(Npc& npc, Player& mostPlayerDamage, int32_t points) {
	Race race = mostPlayerDamage.getRace();
	captureRoom(race, npc.getNpcId() + 14 - 701454);
	for (const runtime::Ptr<Player>& player : instance->getPlayersInside()) {
		PacketSendUtility::sendPacket(*player,
			SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_ROOM_DESTROYED(utils::ChatUtil::l10n(model::getL10nId(race)), npc.getObjectTemplate()->getL10n()));
	}
	if (killedSurkanas.incrementAndGet() == 5) {
		spawn(233371, 485.423f, 808.826f, 416.868f, int8_t{30});
		sendMsgByRace(1401416, Race::PC_ALL, 0);
	}
	getPlayerReward(mostPlayerDamage)->incrementCapturedZones();
	updateScore(mostPlayerDamage, runtime::Ptr<Creature>(npc), points, false);
	npc.getController().delete_();
}

// Java TerathDredgionInstance.java:73-152
void TerathDredgionInstance::onDie(Npc& npc) {
	runtime::Ptr<Player> mostPlayerDamage = npc.getAggroList().getMostPlayerDamage();
	if (mostPlayerDamage == nullptr || instanceScore.get()->getInstanceProgressionType() != InstanceProgressionType::START_PROGRESS) {
		return;
	}
	Race race = mostPlayerDamage->getRace();
	switch (npc.getNpcId()) {
		case 701441:
		case 701442:
			onDieSurkana(npc, *mostPlayerDamage, 400);
			return;
		case 701443:
		case 701451:
		case 701452:
		case 701453:
		case 701454:
			onDieSurkana(npc, *mostPlayerDamage, 700);
			return;
		case 701448:
		case 701449:
			onDieSurkana(npc, *mostPlayerDamage, 800);
			return;
		case 701450:
			onDieSurkana(npc, *mostPlayerDamage, 900);
			return;
		case 701444:
		case 701445:
			onDieSurkana(npc, *mostPlayerDamage, 1000);
			return;
		case 701446:
		case 701447:
			onDieSurkana(npc, *mostPlayerDamage, 1100);
			return;
		case 730572:
			spawn(730566, 446.729f, 493.224f, 395.938f, int8_t{0}, 12);
			npc.getController().delete_();
			return;
		case 730573:
			spawn(730567, 520.404f, 493.261f, 395.938f, int8_t{0}, 133);
			npc.getController().delete_();
			return;
		case 730570:
			sendMsgByRace(1401418, race, 0);
			spawn(730560, 396.979f, 184.392f, 433.940f, int8_t{0}, 42);
			break;
		case 730571:
			sendMsgByRace(1401418, race, 0);
			spawn(730561, 554.64f, 173.535f, 433.940f, int8_t{0}, 9);
			break;
		case 233378: // Master at Arms Vandukar
			sendMsgByRace(1401419, Race::PC_ALL, 0);
			if (race == Race::ASMODIANS) {
				spawn(730563, 496.178f, 761.770f, 390.805f, int8_t{0}, 186);
			} else {
				spawn(730562, 473.759f, 761.864f, 390.805f, int8_t{0}, 33);
			}
			return;
		case 233377: // Enforcer Udara
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 1000, false);
			if (commons::utils::Rnd::nextBoolean()) {
				spawn(701455, 484.500f, 495.700f, 397.425f, int8_t{33});
				sendMsgByRace(1401421, Race::PC_ALL, 0);
			}
			return;
		case 233370:
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 500, false);
			return;
		case 233371: // Captain Anusa
			if (!instanceScore.get()->isRewarded()) {
				updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 1000, false);
				stopInstance(instanceScore.get()->getRaceWithHighestPoints());
			}
			return;
		case 701439:
			updateScore(*mostPlayerDamage, runtime::Ptr<Creature>(npc), 100, false);
			npc.getController().delete_();
			return;
		default:
			break;
	}
	DredgionInstance::onDie(npc);
}

// Java TerathDredgionInstance.java:155-165
void TerathDredgionInstance::doReward(Player& player, PvpInstancePlayerReward& reward, Race winningRace) {
	if (reward.getRace() == winningRace) {
		reward.setReward1(186000242, 1, 0); // CUSTOM: Ceramium Medal
		if (commons::utils::Rnd::chance() < 20)
			reward.setReward2(188950017, 1, 0); // CUSTOM: Special Courier Pass (Abyss Eternal/Lv. 61-65)
	} else {
		reward.setReward1(186000147, 1, 0); // CUSTOM: Mithril Medal
	}
	DredgionInstance::doReward(player, reward, winningRace);
}

// Java TerathDredgionInstance.java:167-170
void TerathDredgionInstance::openFirstDoors() {
	instance->setDoorState(4, true);
	instance->setDoorState(173, true);
}

} // namespace aion::gameserver::handlers::instance::dredgion
