#pragma once

#include "aion/gameserver/model/templates/siegelocation/ArtifactActivation.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.ArtifactActivation. @author Wakizashi */
class ArtifactActivation : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/ArtifactActivation.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::siegelocation
