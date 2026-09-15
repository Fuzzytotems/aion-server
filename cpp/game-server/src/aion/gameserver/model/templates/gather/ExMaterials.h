#pragma once

#include <vector>

#include "aion/gameserver/model/templates/gather/ExMaterials.xml.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.ExMaterials. @author KID */
class ExMaterials : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/gather/ExMaterials.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists and is read-only */
	const std::vector<Material>& getMaterial() const { return material; }
};

} // namespace aion::gameserver::model::templates::gather
