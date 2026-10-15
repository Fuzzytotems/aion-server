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

	/** Java Comparable: `o.rate - rate` (descending rate, int arithmetic wraps) */
	int32_t compareTo(const Material& o) const { return static_cast<int32_t>(static_cast<uint32_t>(o.rate) - static_cast<uint32_t>(rate)); }
};

} // namespace aion::gameserver::model::templates::gather
