#include "aion/gameserver/dataholders/BaseData.h"

namespace aion::gameserver::dataholders {

int32_t BaseData::size() const {
	return static_cast<int32_t>(baseTemplates.size());
}

} // namespace aion::gameserver::dataholders
