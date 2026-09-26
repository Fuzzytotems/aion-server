#include "aion/gameserver/dataholders/TitleData.h"

namespace aion::gameserver::dataholders {

void TitleData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::TitleTemplate& tt : tts)
		titles.insert_or_assign(tt.getTitleId(), &tt);
	// Java: tts = null (the C++ index points into the storage, which stays)
}

const model::templates::TitleTemplate* TitleData::getTitleTemplate(int32_t titleId) const {
	auto it = titles.find(titleId);
	return it != titles.end() ? it->second : nullptr;
}

int32_t TitleData::size() const {
	return static_cast<int32_t>(titles.size());
}

} // namespace aion::gameserver::dataholders
