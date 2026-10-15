#include "aion/gameserver/dataholders/MaterialData.h"

namespace aion::gameserver::dataholders {

void MaterialData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: if (materialTemplates == null) return; (an empty list builds no index either)
	materialsById.clear();
	for (const model::templates::materials::MaterialTemplate& material : materialTemplates) {
		materialsById.insert_or_assign(material.getId(), &material);
		for (const model::templates::materials::MaterialSkill& skill : material.getSkills()) // Java: if (template.getSkills() != null)
			skillIds.insert(skill.getId());
	}
	// Java: materialTemplates = null (the C++ index points into the storage, which stays)
}

const model::templates::materials::MaterialTemplate* MaterialData::getTemplate(int32_t materialId) const {
	auto it = materialsById.find(materialId);
	return it != materialsById.end() ? it->second : nullptr;
}

bool MaterialData::isMaterialSkill(int32_t skillId) const {
	return skillIds.contains(skillId);
}

int32_t MaterialData::size() const {
	return static_cast<int32_t>(materialsById.size());
}

} // namespace aion::gameserver::dataholders
