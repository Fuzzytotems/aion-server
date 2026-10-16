#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/templates/materials/MeshMaterial.h"

namespace aion::gameserver::model::templates::materials {

/**
 * Java com.aionemu.gameserver.model.templates.materials.MeshList.
 * <p>
 * C++: a JAXB class no static data root reaches (xmlgen-report.md, unreachable types), so it is a plain K5 class (fieldmap) without a binder;
 * afterUnmarshal() keeps Java's hook body for a caller that fills the list.
 */
class MeshList {
protected:
	std::vector<MeshMaterial> meshMaterials;
	int32_t worldId = 0;

	std::unordered_map<std::string, int32_t> materialIdsByPath;
	std::unordered_map<int32_t, std::string> pathZones;

public:
	/** Java afterUnmarshal(Unmarshaller, Object): indexes the materials by path and drops the list */
	void afterUnmarshal();

	int32_t getWorldId() const { return worldId; }

	/**
	 * Find material ID for the specific mesh
	 *
	 * @param meshPath Mesh geo path
	 * @return 0 if not found
	 */
	int32_t getMeshMaterialId(std::string_view meshPath) const;

	std::unordered_set<std::string> getMeshPaths() const;

	/** @return the zone name of the mesh, nullopt (Java null) if the path is unknown */
	std::optional<std::string> getZoneName(std::string_view meshPath) const;

	int32_t size() const { return static_cast<int32_t>(materialIdsByPath.size()); }
};

} // namespace aion::gameserver::model::templates::materials
