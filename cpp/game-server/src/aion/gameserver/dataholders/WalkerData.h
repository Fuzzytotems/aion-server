#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/WalkerData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.WalkerData.
 * <p>
 * C++: the index (Java LinkedHashMap, first route id wins) points into the bound `walkerlist` storage, which stays (Java clears the list).
 * addTemplate and saveData belong to the FixPath admin tool, which writes new XML files through JAXB marshalling: the write-back comes after the
 * load path (static-data.md §3.6 item 7), so both stay unported and the index never changes after publication.
 *
 * @author KKnD, Rolandas
 */
class WalkerData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WalkerData.xml.inc"
private:
	detail::LinkedMap<std::string, const model::templates::walker::WalkerTemplate*> walkerlistData;

public:
	int32_t size() const;

	/** @return the walker route, nullptr (Java null) if there is none */
	const model::templates::walker::WalkerTemplate* getWalkerTemplate(std::string_view routeId) const;

	/** Java: appends a template to the list saveData writes (FixPath). Unported: write-back */
	void addTemplate(std::unique_ptr<model::templates::walker::WalkerTemplate> newTemplate);

	/** Java: writes ./data/static_data/npc_walker/generated_npc_walker_<routeId>.xml with JAXB (FixPath). Unported: write-back */
	void saveData(std::string_view routeId);

	/** Java `walkerlistData.values()` (insertion order) */
	std::vector<const model::templates::walker::WalkerTemplate*> getTemplates() const;
};

} // namespace aion::gameserver::dataholders
