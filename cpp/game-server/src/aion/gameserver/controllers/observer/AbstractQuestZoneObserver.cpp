#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/zone/Sphere.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::controllers::observer {

using geoEngine::math::Vector3f;

AbstractQuestZoneObserver::AbstractQuestZoneObserver(model::gameobjects::player::Player& playerValue,
	const model::templates::zone::ZoneTemplate* zoneTemplate)
	: ActionObserver(ObserverType::ALL), player(playerValue), startPos(playerValue.getX(), playerValue.getY(), playerValue.getZ()),
	  startTime(commons::utils::currentTimeMillis()), observedZone(zoneTemplate) {
	oldPos.set(startPos); // Java: startPos.clone()
}

AbstractQuestZoneObserver::~AbstractQuestZoneObserver() = default;

// Java: anonymous Runnable (fieldmap AbstractQuestZoneObserver_Runnable, capturing only this): a lambda pinned to this observer
void AbstractQuestZoneObserver::moved() {
	if (!isRunning.getAndSet(true)) {
		utils::ThreadPoolManager::getInstance().execute(this, [this] {
			auto running = runtime::finally([this]() noexcept { isRunning.set(false); });
			Vector3f currentPos(player->getX(), player->getY(), player->getZ());
			const model::templates::zone::Sphere* sphere = observedZone->getSphere();
			// Java: unboxing (NullPointerException if absent)
			Vector3f center(detail::unbox(sphere->getX(), "sphere.x"), detail::unbox(sphere->getY(), "sphere.y"),
				detail::unbox(sphere->getZ(), "sphere.z"));
			float distance = startPos.distance(currentPos);
			float distanceFromCenter = center.distance(currentPos);
			if (oldPos.get().distance(currentPos) > 1) {
				stepCount = stepCount.get() + 1;
				oldPos = currentPos;
			}
			onMoved(distance, distanceFromCenter, stepCount.get(), commons::utils::currentTimeMillis() - startTime);
		});
	}
}

} // namespace aion::gameserver::controllers::observer
