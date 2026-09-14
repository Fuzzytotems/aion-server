#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/gather/Material.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.Material. @author ATracer */
class Material : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/gather/Material.xml.inc"
public:
	int32_t getL10nId() const override { return nameid; }
};

} // namespace aion::gameserver::model::templates::gather
