#include "aion/gameserver/services/siege/Assault.h"

#include <cmath>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/Assaulter.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services::siege {

namespace {
/** Java: Math.toRadians(angdeg) (angdeg * DEGREES_TO_RADIANS, JDK 9+) */
double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}
} // namespace

// Java Assault.java:30-35
Assault::Assault(Siege& siege)
	: siegeLocation(siege.getSiegeLocation()), boss(siege.getBoss()), locationId(siege.getSiegeLocationId()),
	  worldId(siege.getSiegeLocation()->getWorldId()) {
}

// Java Assault.java:41-44; lambda at Assault.java:43 (fieldmap key siege.Assault@L43:60): the method reference this::handleAssault
void Assault::startAssault(int32_t delay) {
	if (isStarted.compareAndSet(false, true))
		dredgionTask.set(utils::ThreadPoolManager::getInstance().schedule(this, &Assault::handleAssault, static_cast<int64_t>(delay) * 1000));
}

// Java Assault.java:46-53
void Assault::finishAssault(bool captured) {
	if (runtime::FutureRef task = dredgionTask.get(); task && !task->isDone())
		task->cancel(true);
	if (runtime::FutureRef task = spawnTask.get(); task && !task->isDone())
		task->cancel(true);

	onAssaultFinish(captured && siegeLocation->getRace() == model::siege::SiegeRace::BALAUR);
}

// Java Assault.java:59-69
void Assault::spawnAssaulter(model::siege::Assaulter& a, model::gameobjects::siege::SiegeNpc& target) {
	const int32_t headingOffset = a.getHeadingOffset() * 10;
	const float randomDirection = static_cast<float>(commons::utils::Rnd::get(-headingOffset, headingOffset)) / 10.0f + target.getSpawn()->getHeading();
	const double radian = toRadians(randomDirection * 3.0);
	const float x1 = static_cast<float>(target.getX() + std::cos(radian) * a.getDistanceOffset());
	const float y1 = static_cast<float>(target.getY() + std::sin(radian) * a.getDistanceOffset());

	runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> spawn = spawnengine::SpawnEngine::newSiegeSpawn(getWorldId(),
		a.getNpcId(), locationId, model::siege::SiegeRace::BALAUR, model::siege::SiegeModType::ASSAULT, x1, y1, target.getZ() + 0.5f, 0);
	runtime::Ptr<model::gameobjects::Npc> spawned = runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*spawn, 1));
	spawned->getAggroList().addHate(target, 100000);
}

// Java Assault.java:71-75
std::string Assault::getBossNpcL10n() {
	if (boss && boss->getObjectTemplate() != nullptr)
		return boss->getObjectTemplate()->getL10n();
	return "";
}

Assault::~Assault() = default;

} // namespace aion::gameserver::services::siege
