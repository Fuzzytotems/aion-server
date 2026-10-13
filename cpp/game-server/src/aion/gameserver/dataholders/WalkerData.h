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
 * addTemplate and saveData belong to the FixPath admin tool, which writes new XML files through JAXB marshalling (m5j CP5, D14 = (a)); FixPath
 * uses a WalkerData of its own, so the published index never changes after publication.
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

	/** Java: appends a template to the list saveData writes (FixPath, on a WalkerData of its own) */
	void addTemplate(std::unique_ptr<model::templates::walker::WalkerTemplate> newTemplate);

	/** Java: writes ./data/static_data/npc_walker/generated_npc_walker_<routeId>.xml as JAXB does (FixPath), then clears the list */
	void saveData(std::string_view routeId);

	/** Java `walkerlistData.values()` (insertion order) */
	std::vector<const model::templates::walker::WalkerTemplate*> getTemplates() const;
};

} // namespace aion::gameserver::dataholders
