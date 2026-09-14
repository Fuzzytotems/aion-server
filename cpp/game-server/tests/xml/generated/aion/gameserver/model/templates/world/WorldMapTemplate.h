#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold model.templates.world.WorldMapTemplate`.
// It stands in for the hand-written class of the static data port (P4-09); the hook only counts its calls.

#include <cstddef>
#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WorldMapTemplate (test shell). @author Luno */
class WorldMapTemplate {
#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.inc"
public:
	/** Java: getName() */
	const std::string& getName() const { return name.empty() ? cName : name; }
	/** Java: getAiInfo() (aiInfo = AiInfo.DEFAULT when absent) */
	const AiInfo& getAiInfo() const { return aiInfo ? *aiInfo : AiInfo::DEFAULT; }
	const AiInfo* getBoundAiInfo() const { return aiInfo.get(); }
	const std::optional<std::vector<::aion::gameserver::world::zone::ZoneAttributes>>& getFlagValues() const { return flagValues; }

	/** afterUnmarshal calls of all instances (the test binds on one thread) */
	static inline size_t hookCalls = 0;
};

/** Java: flags = ZoneAttributes.fromList(flagValues) needs the ZoneAttributes companion (not part of the slice) */
inline void WorldMapTemplate::afterUnmarshal(xml::LoadContext&, const xml::XmlParent&) {
	++hookCalls;
}

} // namespace aion::gameserver::model::templates::world
