#pragma once

#include <memory>
#include <vector>

#include "aion/gameserver/model/templates/itemset/FullBonus.xml.h"

namespace aion::gameserver::model::templates::itemset {

/** Java com.aionemu.gameserver.model.templates.itemset.FullBonus. @author ATracer */
class FullBonus : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemset/FullBonus.xml.inc"
public:
	/** @return the stat functions of the modifiers element, nullptr (Java null) without one */
	const std::vector<std::unique_ptr<::aion::gameserver::model::stats::calc::functions::StatFunction>>* getModifiers() const {
		return modifiers != nullptr ? &modifiers->getModifiers() : nullptr;
	}
};

} // namespace aion::gameserver::model::templates::itemset
