#include "aion/gameserver/dataholders/RiftData.h"

#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dataholders {

RiftData::RiftData() = default;

RiftData::~RiftData() = default;

void RiftData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// the hook creates RefCounted run-time objects (and SpawnsData writes collection shims): a nested TaskScope when the load runs in one
	// (DataManager), an own one when a holder is bound alone (tests binding templates outside a scope)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	for (const model::templates::rift::RiftTemplate& template_ : riftTemplates)
		rift.put(template_.getId(), model::rift::RiftLocation::create(&template_));
}

int32_t RiftData::size() const {
	return static_cast<int32_t>(rift.size());
}

const detail::LinkedMap<int32_t, runtime::Ref<model::rift::RiftLocation>>& RiftData::getRiftLocations() const {
	return rift;
}

} // namespace aion::gameserver::dataholders
