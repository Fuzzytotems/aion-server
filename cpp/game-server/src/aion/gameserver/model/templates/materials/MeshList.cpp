#include "aion/gameserver/model/templates/materials/MeshList.h"

#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::model::templates::materials {

namespace {
/** Java String.hashCode() over the UTF-16 code units */
int32_t javaHashCode(std::string_view text) {
	uint32_t hash = 0;
	for (char16_t unit : commons::utils::StringUtils::toUtf16(text))
		hash = 31u * hash + unit;
	return static_cast<int32_t>(hash);
}
} // namespace

void MeshList::afterUnmarshal() {
	if (meshMaterials.empty()) // Java: meshMaterials == null (a plain element list is never present and empty)
		return;
	for (MeshMaterial& meshMaterial : meshMaterials) {
		materialIdsByPath.insert_or_assign(meshMaterial.path, meshMaterial.materialId);
		pathZones.insert_or_assign(javaHashCode(meshMaterial.path), meshMaterial.getZoneName());
		meshMaterial.path.clear();
	}
	meshMaterials.clear();
}

int32_t MeshList::getMeshMaterialId(std::string_view meshPath) const {
	auto materialId = materialIdsByPath.find(std::string(meshPath));
	if (materialId == materialIdsByPath.end())
		return 0;
	return materialId->second;
}

std::unordered_set<std::string> MeshList::getMeshPaths() const {
	std::unordered_set<std::string> paths;
	for (const auto& [path, materialId] : materialIdsByPath)
		paths.insert(path);
	return paths;
}

std::optional<std::string> MeshList::getZoneName(std::string_view meshPath) const {
	auto zone = pathZones.find(javaHashCode(meshPath));
	if (zone == pathZones.end())
		return std::nullopt;
	return zone->second;
}

} // namespace aion::gameserver::model::templates::materials
