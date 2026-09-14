#pragma once

#include "aion/gameserver/model/templates/cp/CPRank.xml.h"

namespace aion::gameserver::model::templates::cp {

/** Java com.aionemu.gameserver.model.templates.cp.CPRank. @author Neon */
class CPRank : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/cp/CPRank.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::cp
