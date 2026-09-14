#pragma once

#include "aion/gameserver/model/templates/item/Stigma.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.Stigma. @author ATracer, Neon */
class Stigma : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/Stigma.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
