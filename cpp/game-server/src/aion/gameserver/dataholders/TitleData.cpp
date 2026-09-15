#include "aion/gameserver/dataholders/TitleData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void TitleData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

const model::templates::TitleTemplate* TitleData::getTitleTemplate(int32_t titleId) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
