#include "aion/gameserver/handlers/ai/instance/danuarReliquary/EnragedQueenModorAI.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HEADING_UPDATE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(EnragedQueenModorAI, "enraged_queen_modor");

// Java EnragedQueenModorAI.java:46-53
void EnragedQueenModorAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	addPlatformLocations();
	if (isCrazedModor()) {
		monsterIds = runtime::Array<int32_t>::of({856493, 856494, 856495, 856496, 856497, 856498});
	}
}

// Java EnragedQueenModorAI.java:55-58 (Rnd.get(AttackHandAnimation.values()): one of the three constants)
model::animations::AttackHandAnimation EnragedQueenModorAI::modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) {
	return static_cast<model::animations::AttackHandAnimation>(
		commons::utils::Rnd::nextInt(static_cast<int32_t>(xml::EnumTraits<model::animations::AttackHandAnimation>::names.size())));
}

// Java EnragedQueenModorAI.java:60-63
int32_t EnragedQueenModorAI::modifyInitialSkillDelay(int32_t delay) {
	return 0;
}

// Java EnragedQueenModorAI.java:65-69
void EnragedQueenModorAI::handleAttack(runtime::Ptr<Creature> creature) {
	hpPhases->tryEnterNextPhase(*this);
	AggressiveNpcAI::handleAttack(creature);
}

// Java EnragedQueenModorAI.java:71-115
void EnragedQueenModorAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 100:
			queueSkill(21171, 1); // Grendal's Explosive Wrath
			queueSkill(21169, 1, 4000); // buff
			break;
		case 90:
			queueSkill(21165, 1, 4000); // rend space
			queueSkill(21170, 1, 4000); // Grendal's Rage
			break;
		case 70:
			queueSkill(21165, 2); // rend space
			queueSkill(21176, 1); // electric vengeance
			queueSkill(21229, 1, 4000); // electric wrath
			queueSkill(21742, 1, 4000); // subzero malice
			queueSkill(21743, 1, 4000); // rushing subzero malice
			break;
		case 52:
			queueSkill(21165, 4); // rend space
			queueSkill(21268, 1); // demented scream
			queueSkill(21269, 1, 4000); // spiteful roar
			queueSkill(21742, 1, 4000); // subzero malice
			queueSkill(21743, 1, 4000); // rushing subzero malice
			queueSkill(21165, 5, 4000); // rend space
			queueSkill(21170, 1, 2500); // Grendal's Rage
			break;
		case 25:
			queueSkill(21165, 6); // rend space
			queueSkill(21176, 1); // electric vengeance
			queueSkill(21229, 1); // electric wrath
			queueSkill(21742, 1); // subzero malice
			queueSkill(21743, 1); // rushing subzero malice
			queueSkill(21165, 7, 4000); // rend space
			queueSkill(21170, 1, 0); // Grendal's Rage
			break;
		case 10:
			queueSkill(21165, 8); // rend space
			queueSkill(21176, 1); // electric vengeance
			queueSkill(21229, 1, 4000); // electric wrath
			queueSkill(21165, 9, 4000); // rend space
			queueSkill(21170, 1, 0); // Grendal's Rage
			break;
		default:
			break;
	}
}

// Java EnragedQueenModorAI.java:117-120
float EnragedQueenModorAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage * multiplier.get();
}

// Java EnragedQueenModorAI.java:122-149
void EnragedQueenModorAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	multiplier = 0.5f;
	switch (skillTemplate->getSkillId()) {
		case 21171:
			multiplier = 1.2f;
			if (stage.get() == 0) {
				PacketSendUtility::broadcastMessage(getOwner(), 1500743);
			}
			break;
		case 21165:
			if (skillLevel != 10) {
				stage.set(skillLevel);
				if (skillLevel == 1 || skillLevel == 5 || skillLevel == 7 || skillLevel == 9) {
					PacketSendUtility::broadcastMessage(getOwner(), 1500750);
				} else if (skillLevel == 2 || skillLevel == 6 || skillLevel == 8) {
					nextPosition = getRandomPosFromCenter(8);
					Vector3f next = nextPosition.get();
					spawn(284385, next.x, next.y, next.getZ(), int8_t{0});
				} else if (skillLevel == 4) {
					PacketSendUtility::broadcastMessage(getOwner(), 1500751);
				}
			}
			break;
		case 21176:
			multiplier = 0.35f;
			break;
		default:
			break;
	}
}

// Java EnragedQueenModorAI.java:151-199
void EnragedQueenModorAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	multiplier = 1.0f;
	int32_t curStage = stage.get();
	switch (skillTemplate->getSkillId()) {
		case 21165:
			switch (curStage) {
				case 1:
				case 3:
				case 5:
				case 7:
				case 9:
					if (skillLevel != 10 && skillLevel != 3) {
						spawnAdds(curStage);
					}
					if (shouldUsePlatformSkills(skillLevel)) {
						queueSkill(21742, 1, getNextSkillTimeFor(curStage)); // subzero malice
						queueSkill(21743, 1, getNextSkillTimeFor(curStage)); // rushing subzero malice
					}
					break;
				default:
					break;
			}
			updatePosition(curStage);
			break;
		case 21743:
			switch (curStage) {
				case 1:
				case 3:
				case 5:
				case 7:
				case 9:
					if (shouldUsePlatformSkills(skillLevel)) {
						queueSkill(21165, 10, 5000 - curStage * 350); // rend space
						queueSkill(21179, 1, getNextSkillTimeFor(curStage)); // ice storm
					}
					break;
				case 2:
					if (getLifeStats()->getHpPercentage() >= 55) {
						queueSkill(21165, 3, 4000); // rend space
						queueSkill(21170, 1, 3000); // Grendal's Rage
					}
					break;
				default:
					break;
			}
			break;
		case 21179: { // ice storm
			Vector3f pos = getRandomPosFromCenter(commons::utils::Rnd::nextInt(9));
			spawn(284528, pos.getX(), pos.getY(), pos.getZ(), int8_t{10});
			break;
		}
		default:
			break;
	}
}

// Java EnragedQueenModorAI.java:201-235
void EnragedQueenModorAI::updatePosition(const int32_t curStage) {
	positionUpdateTask.set(ThreadPoolManager::getInstance().schedule({this}, [this, curStage] {
		if (!getOwner().isDead() && getOwner().isSpawned()) {
			switch (curStage) {
				case 1:
				case 3: // switch platforms
				case 5:
				case 7:
				case 9: {
					Vector3f loc = platformLocations.removeAt(curStage > 3 ? commons::utils::Rnd::get(0, 3) : 0);
					platformLocations.add(loc); // re-add to be able to teleport to loc again
					World::getInstance().updatePosition(getOwner(), loc.getX(), loc.getY(), loc.getZ(),
						PositionUtil::getHeadingTowards(loc.getX(), loc.getY(), 256.62f, 257.79f));
					break;
				}
				case 4: {
					int8_t heading;
					runtime::Ptr<Creature> mostHated = getAggroList().getTarget(AggroTarget::MOST_HATED);
					if (mostHated != nullptr)
						heading = PositionUtil::getHeadingTowards(256.62f, 257.79f, mostHated->getX(), mostHated->getY());
					else
						heading = static_cast<int8_t>(commons::utils::Rnd::nextInt(120));
					World::getInstance().updatePosition(getOwner(), 256.62f, 257.79f, 241.79f, heading);
					break;
				}
				case 2:
				case 6:
				case 8: {
					Vector3f next = nextPosition.get();
					World::getInstance().updatePosition(getOwner(), next.getX(), next.getY(), next.getZ(),
						PositionUtil::getHeadingTowards(next.getX(), next.getY(), 256.62f, 257.79f));
					break;
				}
				default:
					break;
			}
			PacketSendUtility::broadcastPacket(getOwner(), SM_HEADING_UPDATE(getOwner()));
			PacketSendUtility::broadcastPacket(getOwner(), SM_FORCED_MOVE(getOwner(), getOwner()));
		}
	}, 500));
}

// Java EnragedQueenModorAI.java:237-268
void EnragedQueenModorAI::spawnAdds(int32_t curStage) {
	runtime::Ref<runtime::Array<int32_t>> ids = monsterIds.get();
	switch (curStage) {
		case 1:
			spawn(ids->get(0), 256.44f, 278.59f, 241.55f, int8_t{90});
			spawn(ids->get(1), 268.56f, 272.88f, 241.55f, int8_t{78});
			spawn(ids->get(2), 243.61f, 272.99f, 241.55f, int8_t{105});
			spawn(ids->get(2), 257.75f, 239.93f, 241.55f, int8_t{32});
			break;
		case 5:
			spawn(ids->get(3), 260.68f, 262.03f, 241.81f, int8_t{75});
			spawn(ids->get(3), 252.21f, 253.48f, 241.8f, int8_t{15});
			spawn(ids->get(4), 246.57f, 273.86f, 241.55f, int8_t{100});
			spawn(ids->get(5), 269.73f, 244.87f, 241.55f, int8_t{48});
			spawn(ids->get(5), 240.87f, 250.35f, 241.55f, int8_t{10});
			break;
		case 7:
			spawn(ids->get(0), 260.68f, 262.03f, 241.81f, int8_t{75});
			spawn(ids->get(0), 252.21f, 253.48f, 241.8f, int8_t{15});
			spawn(ids->get(1), 246.57f, 273.86f, 241.55f, int8_t{100});
			spawn(ids->get(2), 269.73f, 244.87f, 241.55f, int8_t{48});
			spawn(ids->get(2), 240.87f, 250.35f, 241.55f, int8_t{10});
			break;
		case 9:
			spawn(ids->get(0), 260.68f, 262.03f, 241.81f, int8_t{75});
			spawn(ids->get(1), 252.21f, 253.48f, 241.8f, int8_t{15});
			spawn(ids->get(2), 246.57f, 273.86f, 241.55f, int8_t{100});
			spawn(ids->get(3), 269.73f, 244.87f, 241.55f, int8_t{48});
			spawn(ids->get(4), 240.87f, 250.35f, 241.55f, int8_t{10});
			spawn(ids->get(5), 257.75f, 239.93f, 241.55f, int8_t{32});
			break;
		default:
			break;
	}
}

// Java EnragedQueenModorAI.java:270-284
int32_t EnragedQueenModorAI::getNextSkillTimeFor(int32_t stage) {
	switch (stage) {
		case 1:
		case 2:
		case 3:
			return commons::utils::Rnd::get(2, 6) * 1000;
		case 4:
		case 5:
		case 6:
		case 7:
			return commons::utils::Rnd::get(2, 4) * 1000;
		default:
			return commons::utils::Rnd::get(0, 4) * 1000;
	}
}

// Java EnragedQueenModorAI.java:286-292 (Math.toRadians: angdeg / 180.0 * PI)
Vector3f EnragedQueenModorAI::getRandomPosFromCenter(int32_t distance) {
	Vector3f center(256.62f, 257.79f, 241.8f);
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi;
	center.setX(center.x + static_cast<float>(std::cos(angleRadians) * distance));
	center.setY(center.y + static_cast<float>(std::sin(angleRadians) * distance));
	return center;
}

// Java EnragedQueenModorAI.java:294-296
void EnragedQueenModorAI::queueSkill(int32_t id, int32_t lvl) {
	queueSkill(id, lvl, 0);
}

// Java EnragedQueenModorAI.java:298-300
void EnragedQueenModorAI::queueSkill(int32_t id, int32_t lvl, int32_t nextSkillTime) {
	getOwner().queueSkill(id, lvl, nextSkillTime);
}

// Java EnragedQueenModorAI.java:302-304
bool EnragedQueenModorAI::isCrazedModor() {
	return getNpcId() == 234691; // Crazed Modor (Infernal Danuar Reliquary)
}

// Java EnragedQueenModorAI.java:306-313
void EnragedQueenModorAI::addPlatformLocations() {
	platformLocations.clear();
	platformLocations.add(Vector3f(255.49063f, 293.35785f, 253.79933f));
	platformLocations.add(Vector3f(284.359f, 262.854f, 248.76f));
	platformLocations.add(Vector3f(271.169f, 230.531f, 250.955f));
	platformLocations.add(Vector3f(240.273f, 235.181f, 251.153f));
	platformLocations.add(Vector3f(232.448f, 263.886f, 248.642f));
}

// Java EnragedQueenModorAI.java:315-318
bool EnragedQueenModorAI::shouldUsePlatformSkills(int32_t skillLevel) {
	// if another teleport skill(=21165) is queued with level != 10 -> next stage is ready so stop switching platforms
	return !getOwner().hasQueuedSkill([skillLevel](model::skill::NpcSkillEntry& skill) {
		return skill.getSkillId() == 21165 && skill.getSkillLevel() != 10 && skill.getSkillLevel() != skillLevel;
	});
}

// Java EnragedQueenModorAI.java:320-326
void EnragedQueenModorAI::handleDied() {
	runtime::FutureRef task = positionUpdateTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(false);
	}
	AggressiveNpcAI::handleDied();
}

// Java EnragedQueenModorAI.java:328-337
void EnragedQueenModorAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	stage.set(0);
	hpPhases->reset();
	addPlatformLocations();
	World::getInstance().updatePosition(getOwner(), 256.62f, 257.79f, 241.79f, int8_t{90});
	PacketSendUtility::broadcastPacket(getOwner(), SM_HEADING_UPDATE(getOwner()));
	PacketSendUtility::broadcastPacket(getOwner(), SM_FORCED_MOVE(getOwner(), getOwner()));
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
