#include "aion/gameserver/services/VortexService.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/model/vortex/VortexStateType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.VortexService@L38:39
//   com.aionemu.gameserver.services.VortexService@L39:39
//   com.aionemu.gameserver.services.VortexService@L57:44

void VortexService::initVortexLocations() {
	if (configs::main::CustomConfig::VORTEX_ENABLED.load()) {
		for (const auto& [id, loc] : dataholders::DataManager::VORTEX_DATA->getVortexLocations())
			spawn(*loc, model::vortex::VortexStateType::PEACE);

		const auto require = [](const cron::CronExpression* expression) -> const cron::CronExpression& {
			if (expression == nullptr)
				throw runtime::NullPointerException("cronExpression"); // Java: CronService.schedule with a null expression
			return *expression;
		};
		cron::CronService::getInstance().schedule([] { getInstance().startInvasion(0); },
			require(configs::main::CustomConfig::VORTEX_THEOBOMOS_SCHEDULE.load()));
		cron::CronService::getInstance().schedule([] { getInstance().startInvasion(1); },
			require(configs::main::CustomConfig::VORTEX_BRUSTHONIN_SCHEDULE.load()));
	}
}

void VortexService::startInvasion(int32_t id) {
	AION_UNPORTED();
}

void VortexService::stopInvasion(int32_t id) {
	AION_UNPORTED();
}

void VortexService::spawn(model::vortex::VortexLocation& loc, model::vortex::VortexStateType state) {
	AION_UNPORTED();
}

void VortexService::despawn(model::vortex::VortexLocation& loc) {
	AION_UNPORTED();
}

bool VortexService::isInvasionInProgress(int32_t id) {
	AION_UNPORTED();
}

int32_t VortexService::getDuration() {
	AION_UNPORTED();
}

void VortexService::removeDefenderPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void VortexService::removeInvaderPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool VortexService::isInvaderPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool VortexService::isInsideVortexZone(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::vortex::VortexLocation> VortexService::getLocationByRift(int32_t npcId) {
	return getLocationByWorld(npcId == 831141 ? world::getId(world::WorldMapType::BRUSTHONIN) : world::getId(world::WorldMapType::THEOBOMOS));
}

runtime::Ptr<model::vortex::VortexLocation> VortexService::getLocationByWorld(int32_t worldId) {
	if (worldId == world::getId(world::WorldMapType::THEOBOMOS)) {
		const runtime::Ref<model::vortex::VortexLocation>* loc = dataholders::DataManager::VORTEX_DATA->getVortexLocations().get(0);
		return loc != nullptr ? runtime::Ptr<model::vortex::VortexLocation>(*loc) : nullptr;
	} else if (worldId == world::getId(world::WorldMapType::BRUSTHONIN)) {
		const runtime::Ref<model::vortex::VortexLocation>* loc = dataholders::DataManager::VORTEX_DATA->getVortexLocations().get(1);
		return loc != nullptr ? runtime::Ptr<model::vortex::VortexLocation>(*loc) : nullptr;
	} else {
		return nullptr;
	}
}

VortexService& VortexService::getInstance() {
	static VortexService instance; // Java VortexServiceHolder
	return instance;
}

} // namespace aion::gameserver::services
