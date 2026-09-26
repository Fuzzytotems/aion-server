#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/materials/fwd.h"

namespace aion::gameserver::model::templates::materials {

/**
 * Java com.aionemu.gameserver.model.templates.materials.MeshMaterial.
 * <p>
 * C++: a JAXB class no static data root reaches (xmlgen-report.md, unreachable types), so it is a plain K5 class (fieldmap) without a binder;
 * Java's null strings are empty. MeshList reads the protected members like Java's package access.
 *
 * @author Rolandas
 */
class MeshMaterial {
	friend class MeshList; // Java: same-package access to the protected fields

protected:
	int32_t materialId = 0;
	std::string path;

private:
	std::string zoneName;

public:
	const std::string& getZoneName() const { return zoneName; }
};

} // namespace aion::gameserver::model::templates::materials
