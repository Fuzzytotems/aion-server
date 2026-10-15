#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.xml.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"

namespace aion::gameserver::model::templates::itemset {

/** Java com.aionemu.gameserver.model.templates.itemset.ItemSetTemplate. @author ATracer, Antivirus */
class ItemSetTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::stats::calc::StatOwner {
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.xml.inc"
public:
	/** C++ only: templates are immortal, Ref<StatOwner> does not count them (StatOwner.h) */
	void retain() const noexcept override {}
	void release() const noexcept override {}
};

} // namespace aion::gameserver::model::templates::itemset
