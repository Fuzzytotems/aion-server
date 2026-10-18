#include "aion/gameserver/handlers/ai/instance/custom/pvpveMap/ModifiedIronWallAggressiveAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/handler/ReturningEventHandler.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai::instance::custom::pvpveMap {

AION_AI(ModifiedIronWallAggressiveAI, "modified_iron_wall_aggressive");

// Java ModifiedIronWallAggressiveAI.java:30-33
ModifiedIronWallAggressiveAI::ModifiedIronWallAggressiveAI(Npc& owner)
	: AggressiveNpcAI(owner),
	  hpPhases(owner.getNpcId() == 231304 ? runtime::Ptr<gameserver::ai::HpPhases>(gameserver::ai::HpPhases::create(98, {77, 56, 35, 10}))
										  : runtime::Ptr<gameserver::ai::HpPhases>()) {}

// Java ModifiedIronWallAggressiveAI.java:35-40
void ModifiedIronWallAggressiveAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (hpPhases != nullptr)
		hpPhases->tryEnterNextPhase(*this);
}

// Java ModifiedIronWallAggressiveAI.java:42-45
void ModifiedIronWallAggressiveAI::handleHpPhase(int32_t /*phaseHpPercent*/) {
	getOwner().queueSkill(21165, 1, 3000);
}

// Java ModifiedIronWallAggressiveAI.java:47-78
float ModifiedIronWallAggressiveAI::modifyOwnerDamage(float damage, Creature& /*effected*/, runtime::Ptr<Effect> effect) {
	if (::aion::gameserver::custom::pvpmap::PvpMapService::getInstance().isRandomBoss(getOwner())) {
		return damage > 7900 ? static_cast<float>(7700 + commons::utils::Rnd::nextInt(600)) : damage;
	}
	switch (getOwner().getNpcId()) {
		case 218547: // cleric
		case 219197:
		case 218551:
		case 219190:
		case 219169:
		case 218546: // mage
		case 218550:
		case 219196:
		case 219189:
		case 219168:
			if (effect != nullptr) {
				return getRandomDmg(damage, 1.2f, 1.6f);
			} else {
				return getRandomDmg(damage, 2.4f, 3.2f);
			}
		case 219218: // keymaster chookuri (cleric, normal)
		case 219193: // keymaster dabra (mage, hero)
		case 219191: // keymaster niksi (fighter, elite)
		case 219192: // keymaster zumita (assa, elite)
			return damage;
		default:
			if (effect != nullptr) {
				return getRandomDmg(damage, 2.0f, 2.8f);
			} else {
				return getRandomDmg(damage, 1.8f, 2.3f);
			}
	}
}

// Java ModifiedIronWallAggressiveAI.java:80-104
void ModifiedIronWallAggressiveAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == model::stats::container::StatEnum::MAXHP) {
		if (::aion::gameserver::custom::pvpmap::PvpMapService::getInstance().isRandomBoss(getOwner())) {
			stat.setBase(882321);
		} else {
			switch (getOwner().getNpcId()) {
				case 219167:
				case 219169:
				case 219190:
					stat.setBaseRate(3.2f);
					break;
				case 219218:
					stat.setBaseRate(7.0f);
					break;
				case 219193:
				case 219191:
				case 219192:
					break;
				default:
					stat.setBaseRate(1.6f);
			}
		}
	} else {
		AggressiveNpcAI::modifyOwnerStat(stat);
	}
}

// Java ModifiedIronWallAggressiveAI.java:106-122 (the lambdas: tasks pinning this)
void ModifiedIronWallAggressiveAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	if (getOwner().getNpcId() == 231304) {
		switch (skillTemplate->getSkillId()) {
			case 21165:
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					runtime::Ptr<Creature> randomTarget = getAggroList().getTarget(AggroTarget::RANDOM, 26);
					if (randomTarget == nullptr)
						return;
					world::World::getInstance().updatePosition(getOwner(), randomTarget->getX(), randomTarget->getY(), randomTarget->getZ(),
						randomTarget->getHeading());
					PacketSendUtility::broadcastPacketAndReceive(getOwner(), SM_FORCED_MOVE(getOwner(), getOwner()));
					ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().queueSkill(21171, 1, 6000); }, 500);
				}, 500);
				break;
			default:
				break;
		}
	}
}

// Java ModifiedIronWallAggressiveAI.java:124-129
void ModifiedIronWallAggressiveAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	if (hpPhases != nullptr)
		hpPhases->reset();
}

// Java ModifiedIronWallAggressiveAI.java:131-143
void ModifiedIronWallAggressiveAI::handleNotAtHome() {
	if (getOwner().getNpcId() == 231304) {
		world::World::getInstance().updatePosition(getOwner(), getOwner().getSpawn()->getX(), getOwner().getSpawn()->getY(),
			getOwner().getSpawn()->getZ(), getOwner().getSpawn()->getHeading());
		PacketSendUtility::broadcastPacketAndReceive(getOwner(), SM_FORCED_MOVE(getOwner(), getOwner()));
		if (hpPhases != nullptr)
			hpPhases->reset();
		gameserver::ai::handler::ReturningEventHandler::onBackHome(*this);
	} else {
		AggressiveNpcAI::handleNotAtHome();
	}
}

// Java ModifiedIronWallAggressiveAI.java:145-147
float ModifiedIronWallAggressiveAI::getRandomDmg(float damage, float minMultiplier, float maxMultiplier) {
	return damage * commons::utils::Rnd::nextFloat(minMultiplier, maxMultiplier);
}

} // namespace aion::gameserver::handlers::ai::instance::custom::pvpveMap
