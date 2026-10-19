#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/HM_TiamatWeakenedDragonAI.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(HM_TiamatWeakenedDragonAI, "hm_tiamat_weakened_dragon");

// Java HM_TiamatWeakenedDragonAI.java:25-28
HM_TiamatWeakenedDragonAI::HM_TiamatWeakenedDragonAI(Npc& owner) : TiamatWeakenedDragonAI(owner) {
	hpPhases.set(gameserver::ai::HpPhases::create(50, {25, 20, 15, 5}));
}

// Java HM_TiamatWeakenedDragonAI.java:30-46
void HM_TiamatWeakenedDragonAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 50:
			scheduleDivisiveCreations(60000);
			break;
		case 25:
			scheduleInfinitePain();
			spawnGravityCrusher();
			break;
		case 20:
		case 15:
		case 5:
			spawnGravityCrusher();
			break;
	}
}

// Java HM_TiamatWeakenedDragonAI.java:48-61
void HM_TiamatWeakenedDragonAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20922: // Ultimate Atrocity
			spawnAtrocity(25);
			break;
		case 20924: // Ultimate Atrocity
			spawnAtrocity(0);
			break;
		case 20926: // Ultimate Atrocity
			spawnAtrocity(95);
			break;
	}
}

// Java HM_TiamatWeakenedDragonAI.java:63-65
void HM_TiamatWeakenedDragonAI::spawnAtrocity(int32_t heading) {
	spawn(283238, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), static_cast<int8_t>(heading));
}

// Java HM_TiamatWeakenedDragonAI.java:67-87
int32_t HM_TiamatWeakenedDragonAI::calculateAtrocitySkillId() {
	int32_t accumulatedAnglesAndPlayers[2] = {0, 0};

	getKnownList().forEachPlayer([this, &accumulatedAnglesAndPlayers](Player& p) {
		if (PositionUtil::isInRange(getOwner(), p, 46)) {
			accumulatedAnglesAndPlayers[0] += static_cast<int32_t>(std::floor(PositionUtil::calculateAngleFrom(getOwner(), p) + 0.5f)); // Math.round
			accumulatedAnglesAndPlayers[1]++;
		}
	});
	if (accumulatedAnglesAndPlayers[1] == 0)
		return TiamatWeakenedDragonAI::calculateAtrocitySkillId();

	int32_t medianAngle = accumulatedAnglesAndPlayers[0] / accumulatedAnglesAndPlayers[1];
	if (medianAngle <= 30 || medianAngle >= 330)
		return 20924;
	else if (medianAngle < 150)
		return 20922;
	else
		return 20926;
}

// Java HM_TiamatWeakenedDragonAI.java:89-92
void HM_TiamatWeakenedDragonAI::scheduleSinkingSand() {
	spawnTasks.get()->add(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { spawnSinkingSand(); }, 60000, 50000));
}

// Java HM_TiamatWeakenedDragonAI.java:94-99
void HM_TiamatWeakenedDragonAI::spawnSinkingSand() {
	int32_t density = static_cast<int32_t>(std::min((100 - getLifeStats()->getHpPercentage()) * 1.5f, 100.0f));
	int32_t delay = 250;
	for (int32_t i = 0; i < density; i++)
		spawnTasks.get()->add(ThreadPoolManager::getInstance().schedule({this}, [this] { spawnSinkingSandOnRndPos(); }, delay += 250));
}

// Java HM_TiamatWeakenedDragonAI.java:101-107
void HM_TiamatWeakenedDragonAI::spawnSinkingSandOnRndPos() {
	float distance = commons::utils::Rnd::get(60, 420) * 0.1f;
	double radian = (commons::utils::Rnd::get(-900, 900) * 0.1f) * std::numbers::pi / 180.0; // Math.toRadians
	float x = static_cast<float>(std::cos(radian) * distance);
	float y = static_cast<float>(std::sin(radian) * distance);
	spawn(283135, getPosition()->getX() + x, getPosition()->getY() + y, 417.4f, int8_t{0});
}

// Java HM_TiamatWeakenedDragonAI.java:109-120
void HM_TiamatWeakenedDragonAI::scheduleDivisiveCreations(int32_t delay) {
	spawnTasks.get()->add(ThreadPoolManager::getInstance().schedule({this}, [this, delay] {
		if (hasAggro.get()->get() && !isDead() && delay > 30000) {
			spawn(856042, 464.24f, 462.26f, 417.4f, int8_t{18});
			spawn(856042, 542.79f, 465.03f, 417.4f, int8_t{43});
			spawn(856042, 541.79f, 563.71f, 417.4f, int8_t{74});
			spawn(856042, 465.79f, 565.43f, 417.4f, int8_t{100});
			scheduleDivisiveCreations(delay - 10000);
		}
	}, delay));
}

// Java HM_TiamatWeakenedDragonAI.java:122-132
void HM_TiamatWeakenedDragonAI::spawnGravityCrusher() {
	if (gravityTornadoDistanceLeft.isEmpty() || gravityTornadoDistanceRight.isEmpty())
		return;

	int32_t distance = gravityTornadoDistanceRight.removeAt(commons::utils::Rnd::nextInt(gravityTornadoDistanceRight.size()));
	spawnGravityTornadoByDistAndAngle(distance, 330);

	distance = gravityTornadoDistanceLeft.removeAt(commons::utils::Rnd::nextInt(gravityTornadoDistanceLeft.size()));
	spawnGravityTornadoByDistAndAngle(distance, 30);
}

// Java HM_TiamatWeakenedDragonAI.java:134-139
void HM_TiamatWeakenedDragonAI::spawnGravityTornadoByDistAndAngle(int32_t distance, int32_t angle) {
	double radian = angle * std::numbers::pi / 180.0; // Math.toRadians
	float x = static_cast<float>(std::cos(radian) * distance);
	float y = static_cast<float>(std::sin(radian) * distance);
	spawn(856046, getPosition()->getX() + x, getPosition()->getY() + y, 417.4f, int8_t{0});
}

// Java HM_TiamatWeakenedDragonAI.java:141-144
void HM_TiamatWeakenedDragonAI::spawnInfinitePain() {
	spawn(856048, 508.32f, 515.18f, 417.4f, int8_t{0});
}

// Java HM_TiamatWeakenedDragonAI.java:146-153
void HM_TiamatWeakenedDragonAI::despawnAdds() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	deleteNpcs(instance->getNpcs({856045}));
	deleteNpcs(instance->getNpcs({856041}));
	deleteNpcs(instance->getNpcs({856042}));
	deleteNpcs(instance->getNpcs({856046}));
}

// Java HM_TiamatWeakenedDragonAI.java:155-160
void HM_TiamatWeakenedDragonAI::addGravityTornadoSpots() {
	gravityTornadoDistanceLeft.clear();
	gravityTornadoDistanceRight.clear();
	for (int32_t distance : {7, 14, 21, 28, 35, 42}) // Collections.addAll
		gravityTornadoDistanceLeft.add(distance);
	for (int32_t distance : {7, 14, 21, 28, 35, 42})
		gravityTornadoDistanceRight.add(distance);
}

// Java HM_TiamatWeakenedDragonAI.java:162-166
void HM_TiamatWeakenedDragonAI::handleSpawned() {
	TiamatWeakenedDragonAI::handleSpawned();
	addGravityTornadoSpots();
}

// Java HM_TiamatWeakenedDragonAI.java:168-172
void HM_TiamatWeakenedDragonAI::handleBackHome() {
	addGravityTornadoSpots();
	TiamatWeakenedDragonAI::handleBackHome();
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
