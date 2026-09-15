#include "aion/gameserver/dataholders/TribeRelationsData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void TribeRelationsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

model::TribeClass TribeRelationsData::getBaseTribe(model::TribeClass tribeName) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
