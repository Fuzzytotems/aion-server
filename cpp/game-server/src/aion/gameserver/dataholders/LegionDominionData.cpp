#include "aion/gameserver/dataholders/LegionDominionData.h"

namespace aion::gameserver::dataholders {

int32_t LegionDominionData::size() const {
	return static_cast<int32_t>(ldl.size());
}

} // namespace aion::gameserver::dataholders
