#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/teleport/TeleLocIdData.xml.h"

namespace aion::gameserver::model::templates::teleport {

/** Java com.aionemu.gameserver.model.templates.teleport.TeleLocIdData. @author ATracer */
class TeleLocIdData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/teleport/TeleLocIdData.xml.inc"
public:
	/** @return the first location with the id, nullptr (Java null) if there is none */
	const TeleportLocation* getTeleportLocation(int32_t value) const {
		for (const TeleportLocation& t : locids) {
			if (t.getLocId() == value)
				return &t;
		}
		return nullptr;
	}
};

} // namespace aion::gameserver::model::templates::teleport
