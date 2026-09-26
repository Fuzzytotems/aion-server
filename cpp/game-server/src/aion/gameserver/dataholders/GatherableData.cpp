#include "aion/gameserver/dataholders/GatherableData.h"

#include <algorithm>
#include <vector>

namespace aion::gameserver::dataholders {

namespace {

using model::templates::gather::Material;

/**
 * Java `list.sort(null)`: a stable sort by Material.compareTo. The list belongs to the template being bound (not published yet), so the const
 * accessor's storage is sorted in place; materials are plain data without registered XmlIDs, so moving them is safe.
 */
void sortMaterials(const std::vector<Material>& materials) {
	auto& list = const_cast<std::vector<Material>&>(materials);
	std::stable_sort(list.begin(), list.end(), [](const Material& a, const Material& b) { return a.compareTo(b) < 0; });
}

} // namespace

void GatherableData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::gather::GatherableTemplate& gatherable : gatherables) {
		if (gatherable.getMaterials() != nullptr)
			sortMaterials(gatherable.getMaterials()->getMaterial());
		if (gatherable.getExtraMaterials() != nullptr)
			sortMaterials(gatherable.getExtraMaterials()->getMaterial());
		gatherableData.insert_or_assign(gatherable.getTemplateId(), &gatherable);
	}
	// Java: gatherables = null (the C++ index points into the storage, which stays)
}

int32_t GatherableData::size() const {
	return static_cast<int32_t>(gatherableData.size());
}

const model::templates::gather::GatherableTemplate* GatherableData::getGatherableTemplate(int32_t id) const {
	auto it = gatherableData.find(id);
	return it != gatherableData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
