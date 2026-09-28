#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/templates/StorageExpansionTemplate.xml.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.StorageExpansionTemplate. @author Simple */
class StorageExpansionTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/StorageExpansionTemplate.xml.inc"
public:
	/** @return the lowest expansion level, 0 without expansions */
	int32_t getMinExpansionLevel() const;

	/** @return the highest expansion level, 0 without expansions */
	int32_t getMaxExpansionLevel() const;

	/** @return the price of the first expansion of the level, nullopt (Java null) if there is none */
	std::optional<int32_t> getPrice(int32_t level) const;
};

} // namespace aion::gameserver::model::templates
