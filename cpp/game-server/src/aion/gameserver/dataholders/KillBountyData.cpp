#include "aion/gameserver/dataholders/KillBountyData.h"

namespace aion::gameserver::dataholders {

int32_t KillBountyData::size() const {
	return static_cast<int32_t>(killBounties.size());
}

} // namespace aion::gameserver::dataholders
