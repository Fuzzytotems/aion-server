#include "aion/gameserver/dataholders/PetData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void PetData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

const model::templates::pet::PetTemplate* PetData::getPetTemplate(int32_t id) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
