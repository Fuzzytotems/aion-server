#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/siegelocation/ArtifactActivation.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.ArtifactActivation. @author Wakizashi */
class ArtifactActivation : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/ArtifactActivation.xml.inc"
public:
	/** Java: cd * 1000 (int arithmetic, widened to long) */
	int64_t getCd() const { return static_cast<int32_t>(static_cast<uint32_t>(cd) * 1000u); }
};

} // namespace aion::gameserver::model::templates::siegelocation
