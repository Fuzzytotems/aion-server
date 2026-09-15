#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/AssembledNpcsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.AssembledNpcsData.
 * <p>
 * C++: the index points into the bound `templates` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class AssembledNpcsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AssembledNpcsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::assemblednpc::AssembledNpcTemplate*> assembledNpcsTemplates;

public:
	int32_t size() const;

	/** @return the template, nullptr (Java null) if there is none */
	const model::templates::assemblednpc::AssembledNpcTemplate* getAssembledNpcTemplate(int32_t i) const;
};

} // namespace aion::gameserver::dataholders
