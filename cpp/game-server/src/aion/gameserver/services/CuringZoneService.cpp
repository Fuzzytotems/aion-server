#include "aion/gameserver/services/CuringZoneService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/CuringObjectsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/curingzone/CuringObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/curingzones/CuringTemplate.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.CuringZoneService");

CuringZoneService::CuringZoneService() {
	for (const model::templates::curingzones::CuringTemplate* t : dataholders::DataManager::CURING_OBJECTS_DATA->getCuringObject()) {
		runtime::Ref<model::curingzone::CuringObject> obj = model::gameobjects::VisibleObject::create<model::curingzone::CuringObject>(t, 0);
		obj->spawn();
		curingObjects.add(obj);
	}
	log.info("spawned Curing Zones");
	startTask();
}

CuringZoneService::~CuringZoneService() = default;

CuringZoneService& CuringZoneService::getInstance() {
	static CuringZoneService instance; // Java SingletonHolder
	return instance;
}

// anonymous Runnable at CuringZoneService.java:38 (fieldmap key CuringZoneService$1); argument 1 of scheduleAtFixedRate(); storage: task
// anonymous Consumer at CuringZoneService.java:43 (fieldmap key CuringZoneService$2); argument 1 of forEachPlayer(); storage: sync
void CuringZoneService::startTask() {
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		for (const runtime::Ptr<model::curingzone::CuringObject>& obj : curingObjects) {
			obj->getKnownList().forEachPlayer([&obj](model::gameobjects::player::Player& player) {
				if (utils::PositionUtil::isInRange(*obj, player, obj->getRange()) && !player.getEffectController()->hasAbnormalEffect(8751)) {
					skillengine::SkillEngine::getInstance().getSkill(player, 8751, 1, runtime::Ptr<model::gameobjects::VisibleObject>(player))->useNoAnimationSkill();
				}
			});
		}
	}, 1000, 1000);
}

} // namespace aion::gameserver::services
