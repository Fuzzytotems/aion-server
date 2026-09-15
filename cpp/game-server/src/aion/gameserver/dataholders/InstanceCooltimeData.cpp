#include "aion/gameserver/dataholders/InstanceCooltimeData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void InstanceCooltimeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

int32_t InstanceCooltimeData::getInstanceMaxCountByWorldId(int32_t worldId) const {
	AION_UNPORTED();
}

int64_t InstanceCooltimeData::calculateInstanceEntranceCooltime(model::gameobjects::player::Player& player, int32_t worldId) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
