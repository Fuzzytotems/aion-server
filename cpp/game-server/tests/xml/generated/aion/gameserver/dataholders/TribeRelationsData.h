#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold dataholders.TribeRelationsData`. It
// stands in for the hand-written class of the static data port (P4-09) and ports only the index built by the hook.

#include <cstddef>
#include <unordered_map>

#include "aion/gameserver/dataholders/TribeRelationsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TribeRelationsData (test shell). @author ATracer */
class TribeRelationsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TribeRelationsData.xml.inc"
public:
	/** Java: size() */
	size_t size() const { return tribeNameMap.size(); }
	const model::templates::tribe::Tribe* getTribe(model::TribeClass name) const {
		auto it = tribeNameMap.find(name);
		return it == tribeNameMap.end() ? nullptr : it->second;
	}

private:
	std::unordered_map<model::TribeClass, const model::templates::tribe::Tribe*> tribeNameMap;
};

/** Java: tribeNameMap.put(tribe.getName(), tribe) */
inline void TribeRelationsData::afterUnmarshal(xml::LoadContext&, const xml::XmlParent&) {
	for (const model::templates::tribe::Tribe& tribe : tribeList)
		tribeNameMap[tribe.getName()] = &tribe;
}

} // namespace aion::gameserver::dataholders
