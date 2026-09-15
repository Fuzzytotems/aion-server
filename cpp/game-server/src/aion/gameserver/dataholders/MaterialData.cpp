#include "aion/gameserver/dataholders/MaterialData.h"

namespace aion::gameserver::dataholders {

void MaterialData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	materialsById.clear();
	for (const model::templates::materials::MaterialTemplate& material : materialTemplates)
		materialsById.insert_or_assign(material.getId(), &material);
	// Java also collects the ids of the material skills into skillIds (isMaterialSkill, P4-09); materialTemplates = null (the C++ index points
	// into the storage, which stays)
}

const model::templates::materials::MaterialTemplate* MaterialData::getTemplate(int32_t materialId) const {
	auto it = materialsById.find(materialId);
	return it != materialsById.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
