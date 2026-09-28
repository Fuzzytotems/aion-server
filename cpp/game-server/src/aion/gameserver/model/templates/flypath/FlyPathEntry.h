#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/flypath/FlyPathEntry.xml.h"

#include "aion/gameserver/model/templates/detail/JavaCasts.h"

namespace aion::gameserver::model::templates::flypath {

/** Java com.aionemu.gameserver.model.templates.flypath.FlyPathEntry. @author KID */
class FlyPathEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.xml.inc"
public:
	/** Java: (int) (time * 1000) */
	int32_t getTimeInMs() const { return templates::detail::floatToInt(time * 1000.0f); }
};

} // namespace aion::gameserver::model::templates::flypath
