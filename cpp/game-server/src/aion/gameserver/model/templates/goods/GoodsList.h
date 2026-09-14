#pragma once

#include "aion/gameserver/model/templates/goods/GoodsList.xml.h"

namespace aion::gameserver::model::templates::goods {

/** Java com.aionemu.gameserver.model.templates.goods.GoodsList. @author ATracer */
class GoodsList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/goods/GoodsList.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::goods
