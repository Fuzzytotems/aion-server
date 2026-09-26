#pragma once

#include <string>
#include <unordered_set>

#include "aion/gameserver/dataholders/GlobalNpcExclusionData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.GlobalNpcExclusionData. @author bobobear */
class GlobalNpcExclusionData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.xml.inc"
private:
	bool isEmpty_ = false;

public:
	/** @return the excluded npc ids, an empty set (Java Collections.emptySet()) if there is no npc_ids element */
	const std::unordered_set<int32_t>& getNpcIds() const;

	const std::unordered_set<std::string>& getNpcNames() const;

	const std::unordered_set<model::templates::npc::NpcTemplateType>& getNpcTemplateTypes() const;

	const std::unordered_set<model::TribeClass>& getNpcTribes() const;

	const std::unordered_set<model::templates::npc::AbyssNpcType>& getNpcAbyssTypes() const;

	bool isEmpty() const;
};

} // namespace aion::gameserver::dataholders
