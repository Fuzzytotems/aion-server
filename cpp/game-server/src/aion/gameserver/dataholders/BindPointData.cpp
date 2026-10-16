#include "aion/gameserver/dataholders/BindPointData.h"

namespace aion::gameserver::dataholders {

void BindPointData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::BindPointTemplate& bind : bplist)
		bindplistData.insert_or_assign(bind.getNpcId(), &bind);
	// Java: bplist = null (the C++ index points into the storage, which stays)
}

int32_t BindPointData::size() const {
	return static_cast<int32_t>(bindplistData.size());
}

const model::templates::BindPointTemplate* BindPointData::getBindPointTemplate(int32_t npcId) const {
	auto it = bindplistData.find(npcId);
	return it != bindplistData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
