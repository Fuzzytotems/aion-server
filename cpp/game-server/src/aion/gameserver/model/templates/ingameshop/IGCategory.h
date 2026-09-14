#pragma once

#include "aion/gameserver/model/templates/ingameshop/IGCategory.xml.h"

namespace aion::gameserver::model::templates::ingameshop {

/** Java com.aionemu.gameserver.model.templates.ingameshop.IGCategory. */
class IGCategory : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/ingameshop/IGCategory.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::ingameshop
