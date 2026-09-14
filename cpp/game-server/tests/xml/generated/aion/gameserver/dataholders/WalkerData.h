#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold dataholders.WalkerData`. It stands in
// for the hand-written class of the static data port (P4-09) and ports only the first-wins route index built by the hook.

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/dataholders/WalkerData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WalkerData (test shell). @author KKnD, Rolandas */
class WalkerData {
#include "aion/gameserver/dataholders/WalkerData.xml.inc"
public:
	/** Java: size() */
	size_t size() const { return walkerlistData.size(); }
	/** Java: getWalkerTemplate(String) (nullptr for unknown ids) */
	const model::templates::walker::WalkerTemplate* getWalkerTemplate(const std::string& routeId) const {
		auto it = walkerlistData.find(routeId);
		return it == walkerlistData.end() ? nullptr : it->second;
	}
	const std::vector<model::templates::walker::WalkerTemplate>& getBoundTemplates() const { return walkerlist; }
	size_t getDuplicateRoutes() const { return duplicates; }

private:
	std::unordered_map<std::string, const model::templates::walker::WalkerTemplate*> walkerlistData;
	size_t duplicates = 0;
};

/** Java: walkerlistData.putIfAbsent(route.getRouteId(), route), warning on duplicates */
inline void WalkerData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent&) {
	for (const model::templates::walker::WalkerTemplate& route : walkerlist) {
		if (!walkerlistData.try_emplace(route.getRouteId(), &route).second) {
			++duplicates;
			ctx.warn("Duplicate route ID: " + route.getRouteId());
		}
	}
}

} // namespace aion::gameserver::dataholders
