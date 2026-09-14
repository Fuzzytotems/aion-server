#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::world::zone {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.zone.ZoneName");

/**
 * Java class initialization: `NONE = new ZoneName("NONE")`, then the static block `zoneNames.put(NONE.name(), NONE)`. zoneNames is an inline
 * static member defined (and therefore initialized) before this definition in this translation unit. The put runs in a STARTUP TaskScope
 * because ConcurrentHashMap operations need one (C2); nothing is borrowed, so the scope pins no reclamation. The object is immortal (never
 * deleted).
 */
const ZoneName* const ZoneName::NONE = [] {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	const ZoneName* none = new ZoneName("NONE");
	zoneNames.put(none->name(), none);
	return none;
}();

ZoneName::ZoneName(std::string_view name) : _name(name) {
}

int32_t ZoneName::id() {
	AION_UNPORTED();
}

const ZoneName* ZoneName::createOrGet(std::string_view name) {
	AION_UNPORTED();
}

int32_t ZoneName::getId(std::string_view name) {
	AION_UNPORTED();
}

const ZoneName* ZoneName::get(std::string_view name) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::zone
