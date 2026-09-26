#include "aion/gameserver/world/zone/ZoneName.h"

#include <optional>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
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

int32_t ZoneName::id() const {
	// Java: _name.hashCode() (String.hashCode over the UTF-16 code units, int arithmetic wraps)
	uint32_t hash = 0;
	for (char16_t c : commons::utils::StringUtils::toUtf16(_name))
		hash = 31 * hash + c;
	return static_cast<int32_t>(hash);
}

const ZoneName* ZoneName::createOrGet(std::string_view name) {
	// Called by the zone template XML binding (ZoneTemplate::setXmlName), which may run on a thread without a task (static data parsing): the
	// map operation needs a TaskScope (C2), so one is opened when none is active. Nothing is borrowed beyond the call.
	std::optional<runtime::TaskScope> scope;
	if (!runtime::TaskScope::active())
		scope.emplace(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	const ZoneName* zoneName =
		zoneNames.computeIfAbsent(commons::utils::StringUtils::toUpperCase(name), [](const std::string& upperName) -> const ZoneName* {
			return new ZoneName(upperName); // immortal (interned for the life of the process, like Java's static map)
		});
	return zoneName;
}

int32_t ZoneName::getId(std::string_view name) {
	return zoneNames.getOrDefault(commons::utils::StringUtils::toUpperCase(name), NONE)->id();
}

const ZoneName* ZoneName::get(std::string_view name) {
	std::string upperName = commons::utils::StringUtils::toUpperCase(name);
	const ZoneName* zoneName = zoneNames.get(upperName);
	if (!zoneName) {
		log.warn("Missing zone : " + upperName);
		return NONE;
	}
	return zoneName;
}

} // namespace aion::gameserver::world::zone
