#include "aion/gameserver/handlers/ai/instance/danuarReliquary/CursedQueenModorAI.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HEADING_UPDATE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(CursedQueenModorAI, "cursed_queen_modor");

// Java CursedQueenModorAI.java:41-45
void CursedQueenModorAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	addPlatformLocations();
}

// Java CursedQueenModorAI.java:47-50 (Rnd.get(AttackHandAnimation.values()): one of the three constants)
model::animations::AttackHandAnimation CursedQueenModorAI::modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) {
	return static_cast<model::animations::AttackHandAnimation>(
		commons::utils::Rnd::nextInt(static_cast<int32_t>(xml::EnumTraits<model::animations::AttackHandAnimation>::names.size())));
}

// Java CursedQueenModorAI.java:52-56
void CursedQueenModorAI::handleAttack(runtime::Ptr<Creature> creature) {
	hpPhases->tryEnterNextPhase(*this);
	AggressiveNpcAI::handleAttack(creature);
}

// Java CursedQueenModorAI.java:58-81
void CursedQueenModorAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 100:
			queueSkill(21181, 1); // Malevolence
			queueSkill(21171, 1); // Grendal's Explosive Wrath
			break;
		case 81:
			PacketSendUtility::broadcastMessage(getOwner(), 1500741);
			queueSkill(21171, 1); // Grendal's Explosive Wrath
			queueSkill(21229, 1); // Dragon Lords Lightning Shock
			break;
		case 77:
			queueSkill(21165, 2, 4000);
			queueSkill(21179, 1, 4000); // ice storm when going up 1st time
			break;
		case 61:
			queueSkill(21165, 3);
			break;
		case 50:
			queueSkill(21175, 4);
			break;
		default:
			break;
	}
}

// Java CursedQueenModorAI.java:83-86
float CursedQueenModorAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage * multiplier.get(); // fix skill dmg until new calculations are available
}

// Java CursedQueenModorAI.java:88-122
void CursedQueenModorAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 21171:
			multiplier = 1.2f;
			break;
		case 21165: // teleport
			if (skillLevel != 10) {
				stage.set(skillLevel);
				if (skillLevel == 2) {
					PacketSendUtility::broadcastMessage(getOwner(), 1500750);
				} else if (skillLevel == 3) {
					nextPosition = getRandomPosFromCenter(8);
					Vector3f next = nextPosition.get();
					spawn(284385, next.x, next.y, next.getZ(), int8_t{0});
				}
			}
			break;
		case 21181: // Malevolence, buff
			stage.set(1);
			PacketSendUtility::broadcastMessage(getOwner(), 1500740);
			break;
		case 21175: // Freezing Sphere of Revenge
			if (skillLevel == 4) { // use this skill to despawn modor and spawn clones
				stage.set(4);
				PacketSendUtility::broadcastMessage(getOwner(), 1500742);
			}
			multiplier = 0.5f;
			break;
		case 21174:
		case 21229:
		case 21173:
			multiplier = 0.5f;
			break;
		default:
			break;
	}
}

// Java CursedQueenModorAI.java:124-193
void CursedQueenModorAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	int32_t curStage = stage.get();
	switch (skillTemplate->getSkillId()) {
		case 21165: // teleport
			switch (curStage) {
				case 2:
					if (skillLevel != 10) {
						spawnAdds();
					} else {
						queueSkill(21179, 1, commons::utils::Rnd::get(3000, 5000)); // ice storm when going up 1st time
					}
					break;
				case 3:
					queueSkill(21229, 1); //  Dragon Lords Lightning Shock
					break;
				default:
					break;
			}
			updatePosition(curStage);
			break;
		case 21174: // frozen domain of rancour
			multiplier = 1.0f;
			if (skillLevel == 1 && (curStage == 1 || curStage == 5)) {
				float rnd = commons::utils::Rnd::chance();
				if (rnd < 15) {
					queueSkill(21175, 2, 5000); // lv 2 prevent skill loop
				} else if (rnd < 45) {
					queueSkill(21173, 1, 5000); // summon frost storm
				}
			}
			break;
		case 21175: // frozen domain of revenge
			multiplier = 1.0f;
			switch (curStage) {
				case 2: // going up 1st time, switch platforms
					if (shouldUsePlatformSkills(skillLevel)) {
						queueSkill(21165, 10, 5000);
					}
					break;
				case 4: // = hp <= 50%
					spawnClones();
					getOwner().getController().delete_();
					break;
				case 1:
				case 3: // when she's not on a platform/last stage use aoe skills
					if (skillLevel == 1) {
						float rnd = commons::utils::Rnd::chance();
						if (rnd < 15) {
							queueSkill(21174, 2, 5000); // lv 2 to prevent skill loop
						} else if (rnd < 45) {
							queueSkill(21229, 1, 5000); //  Dragon Lords Lightning Shock
						}
					}
					break;
				default:
					break;
			}
			break;
		case 21179: { // ice storm
			Vector3f pos = getRandomPosFromCenter(commons::utils::Rnd::nextInt(9));
			spawn(284528, pos.getX(), pos.getY(), pos.getZ(), int8_t{10});
			if (curStage == 2 && shouldUsePlatformSkills(skillLevel)) {
				queueSkill(21174, 1, 5000); // frozen domain of rancour
				queueSkill(21175, 1, 5000); // frozen domain of revenge
			}
			break;
		}
		default:
			multiplier = 1.0f;
			break;
	}
}

// Java CursedQueenModorAI.java:195-226
void CursedQueenModorAI::spawnClones() {
	int32_t spawnCase = commons::utils::Rnd::get(1, 5);
	// spawn real clone
	switch (spawnCase) {
		case 1:
			spawn(getRealCloneId(), 255.5489f, 293.42154f, 253.78925f, int8_t{90});
			break;
		case 2:
			spawn(getRealCloneId(), 232.5363f, 263.90112f, 248.65384f, int8_t{114});
			break;
		case 3:
			spawn(getRealCloneId(), 240.11194f, 235.08876f, 251.14906f, int8_t{17});
			break;
		case 4:
			spawn(getRealCloneId(), 271.23627f, 230.30913f, 250.92981f, int8_t{42});
			break;
		case 5:
			spawn(getRealCloneId(), 284.6919f, 262.7201f, 248.75252f, int8_t{63});
			break;
		default:
			break;
	}
	// spawn fake clones
	if (spawnCase != 1)
		spawn(getFakeCloneId(), 255.5489f, 293.42154f, 253.78925f, int8_t{90});
	if (spawnCase != 2)
		spawn(getFakeCloneId(), 232.5363f, 263.90112f, 248.65384f, int8_t{114});
	if (spawnCase != 3)
		spawn(getFakeCloneId(), 240.11194f, 235.08876f, 251.14906f, int8_t{17});
	if (spawnCase != 4)
		spawn(getFakeCloneId(), 271.23627f, 230.30913f, 250.92981f, int8_t{42});
	if (spawnCase != 5)
		spawn(getFakeCloneId(), 284.6919f, 262.7201f, 248.75252f, int8_t{63});
}

// Java CursedQueenModorAI.java:228-232
void CursedQueenModorAI::spawnAdds() {
	spawn(284380, 266.26517f, 273.97614f, 241.54623f, int8_t{83});
	spawn(284381, 256.57727f, 278.18225f, 241.54623f, int8_t{90});
	spawn(284382, 246.65663f, 275.51996f, 241.54623f, int8_t{96});
}

// Java CursedQueenModorAI.java:234-236
void CursedQueenModorAI::queueSkill(int32_t id, int32_t lvl) {
	queueSkill(id, lvl, 0);
}

// Java CursedQueenModorAI.java:238-240
void CursedQueenModorAI::queueSkill(int32_t id, int32_t lvl, int32_t nextSkillTime) {
	getOwner().queueSkill(id, lvl, nextSkillTime);
}

// Java CursedQueenModorAI.java:242-266
void CursedQueenModorAI::updatePosition(const int32_t curStage) {
	ThreadPoolManager::getInstance().schedule({this}, [this, curStage] {
		if (!getOwner().isDead() && getOwner().isSpawned()) {
			switch (curStage) {
				case 1: // coming down for 1st time
					World::getInstance().updatePosition(getOwner(), 256.62f, 257.79f, 241.79f, int8_t{90});
					break;
				case 2: { // going up 1st time // switch platform
					Vector3f loc = platformLocations.removeAt(0);
					platformLocations.add(loc); // re-add to be able to teleport to loc again
					World::getInstance().updatePosition(getOwner(), loc.getX(), loc.getY(), loc.getZ(),
						PositionUtil::getHeadingTowards(loc.getX(), loc.getY(), 256.62f, 257.79f)); // heading towards center
					break;
				}
				case 3: { // coming down 2nd time: spawn at random loc
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
	}, 500);
}

// Java CursedQueenModorAI.java:268-275
void CursedQueenModorAI::addPlatformLocations() {
	platformLocations.clear();
	platformLocations.add(Vector3f(255.49063f, 293.35785f, 253.79933f));
	platformLocations.add(Vector3f(284.359f, 262.854f, 248.76f));
	platformLocations.add(Vector3f(271.169f, 230.531f, 250.955f));
	platformLocations.add(Vector3f(240.273f, 235.181f, 251.153f));
	platformLocations.add(Vector3f(232.448f, 263.886f, 248.642f));
}

// Java CursedQueenModorAI.java:277-280
int32_t CursedQueenModorAI::modifyInitialSkillDelay(int32_t delay) {
	return 0;
}

// Java CursedQueenModorAI.java:282-291
void CursedQueenModorAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	stage.set(0);
	hpPhases->reset();
	addPlatformLocations();
	World::getInstance().updatePosition(getOwner(), 256.62f, 257.79f, 241.79f, int8_t{90});
	PacketSendUtility::broadcastPacket(getOwner(), SM_HEADING_UPDATE(getOwner()));
	PacketSendUtility::broadcastPacket(getOwner(), SM_FORCED_MOVE(getOwner(), getOwner()));
}

// Java CursedQueenModorAI.java:293-296
bool CursedQueenModorAI::shouldUsePlatformSkills(int32_t skillLevel) {
	// if another teleport skill(=21165) is queued with level != 10 -> next stage is ready so stop switching platforms
	return !getOwner().hasQueuedSkill([skillLevel](model::skill::NpcSkillEntry& skill) {
		return skill.getSkillId() == 21165 && skill.getSkillLevel() != 10 && skill.getSkillLevel() != skillLevel;
	});
}

// Java CursedQueenModorAI.java:298-300
int32_t CursedQueenModorAI::getRealCloneId() {
	return getNpcId() == 234690 ? 855244 : 284383;
}

// Java CursedQueenModorAI.java:302-304
int32_t CursedQueenModorAI::getFakeCloneId() {
	return getNpcId() == 234690 ? 855245 : 284384;
}

// Java CursedQueenModorAI.java:306-312 (Math.toRadians: angdeg / 180.0 * PI)
Vector3f CursedQueenModorAI::getRandomPosFromCenter(int32_t distance) {
	Vector3f center(256.62f, 257.79f, 241.8f);
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi;
	center.setX(center.x + static_cast<float>(std::cos(angleRadians) * distance));
	center.setY(center.y + static_cast<float>(std::sin(angleRadians) * distance));
	return center;
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
