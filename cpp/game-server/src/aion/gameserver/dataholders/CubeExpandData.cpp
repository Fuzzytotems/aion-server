#include "aion/gameserver/dataholders/CubeExpandData.h"

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

void CubeExpandData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::StorageExpansionTemplate& expansionTemplate : expansionTemplates) {
		if (!expansionTemplate.getNpcIds())
			throw runtime::NullPointerException("StorageExpansionTemplate.npcIds"); // Java: iterating the null list of a template without npc ids
		for (int32_t npcId : *expansionTemplate.getNpcIds())
			expansionTemplatesByNpcId.insert_or_assign(npcId, &expansionTemplate);
	}
	// Java: expansionTemplates = null (the C++ index points into the storage, which stays)
}

int32_t CubeExpandData::size() const {
	return static_cast<int32_t>(expansionTemplatesByNpcId.size());
}

const model::templates::StorageExpansionTemplate* CubeExpandData::getCubeExpansionTemplate(int32_t id) const {
	auto it = expansionTemplatesByNpcId.find(id);
	return it != expansionTemplatesByNpcId.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
