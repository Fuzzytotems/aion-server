#include "aion/gameserver/dataholders/PetBuffsData.h"

namespace aion::gameserver::dataholders {

void PetBuffsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: if (buffs == null) return; (an empty list builds no index either)
	for (const model::templates::pet::PetBuff& buff : buffs)
		petBuffsById.insert_or_assign(buff.getId(), &buff);
	// Java: buffs.clear(); buffs = null (the C++ index points into the storage, which stays)
}

const model::templates::pet::PetBuff* PetBuffsData::getPetBuff(int32_t buffId) const {
	auto it = petBuffsById.find(buffId);
	return it != petBuffsById.end() ? it->second : nullptr;
}

int32_t PetBuffsData::size() const {
	return static_cast<int32_t>(petBuffsById.size());
}

} // namespace aion::gameserver::dataholders
