#include "aion/gameserver/model/templates/event/Buff.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::model::templates::event {

// Java: LoggerFactory.getLogger(Buff.class) inline in afterUnmarshal
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.templates.event.Buff");

void Buff::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	size_t skillIdCount = skillIds ? skillIds->size() : 0; // required attribute: always present after binding
	if (pool > static_cast<int64_t>(skillIdCount)) {
		// Deviation: the ids are listed in ascending order; Java prints the HashSet in its hash order (log text only, docs/deviations/P4-07b.md)
		std::vector<int32_t> ids = skillIds ? std::vector<int32_t>(skillIds->begin(), skillIds->end()) : std::vector<int32_t>();
		std::ranges::sort(ids);
		std::string list;
		for (int32_t id : ids)
			list += (list.empty() ? "" : ", ") + std::to_string(id);
		log.warn("Pool size for event buffs must be smaller than skill id size (skill ids: [" + list + "]).");
	}
}

} // namespace aion::gameserver::model::templates::event
