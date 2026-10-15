#include "aion/gameserver/dataholders/VortexData.h"

#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dataholders {

using model::vortex::VortexLocation;

VortexData::VortexData() = default;

VortexData::~VortexData() = default;

void VortexData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// the hook creates RefCounted run-time objects (and SpawnsData writes collection shims): a nested TaskScope when the load runs in one
	// (DataManager), an own one when a holder is bound alone (tests binding templates outside a scope)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	for (const model::templates::vortex::VortexTemplate& template_ : vortexTemplates)
		vortex.put(template_.getId(), VortexLocation::create(&template_));
}

int32_t VortexData::size() const {
	return static_cast<int32_t>(vortex.size());
}

runtime::Ptr<VortexLocation> VortexData::getVortexLocation(int32_t invasionWorldId) const {
	for (const auto& [id, loc] : vortex) {
		if (loc->getInvasionWorldId() == invasionWorldId)
			return loc;
	}
	return nullptr;
}

const detail::LinkedMap<int32_t, runtime::Ref<VortexLocation>>& VortexData::getVortexLocations() const {
	return vortex;
}

} // namespace aion::gameserver::dataholders
