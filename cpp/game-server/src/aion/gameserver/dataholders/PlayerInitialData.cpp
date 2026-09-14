#include "aion/gameserver/dataholders/PlayerInitialData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void PlayerInitialData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
