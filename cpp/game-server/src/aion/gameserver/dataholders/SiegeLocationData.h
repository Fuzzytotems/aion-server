#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/SiegeLocationData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SiegeLocationData.
 * <p>
 * C++: the siege locations are RefCounted run-time objects (P5-12a) created by the hook and owned by the published holder; Java's
 * LinkedHashMaps are detail::LinkedMaps (read-only after publish). A template without a type (Java: switch on null) fails with
 * NullPointerException.
 *
 * @author Sarynth, antness
 */
class SiegeLocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SiegeLocationData.xml.inc"
private:
	detail::LinkedMap<int32_t, runtime::Ref<model::siege::ArtifactLocation>> artifactLocations;
	detail::LinkedMap<int32_t, runtime::Ref<model::siege::FortressLocation>> fortressLocations;
	detail::LinkedMap<int32_t, runtime::Ref<model::siege::OutpostLocation>> outpostLocations;
	detail::LinkedMap<int32_t, runtime::Ref<model::siege::SiegeLocation>> siegeLocations;
	runtime::Ref<model::siege::AgentLocation> agentLoc;

public:
	/** C++ only: out of line, where the RefCounted element types are complete */
	SiegeLocationData();
	~SiegeLocationData();

	int32_t size() const;

	const detail::LinkedMap<int32_t, runtime::Ref<model::siege::ArtifactLocation>>& getArtifacts() const;

	const detail::LinkedMap<int32_t, runtime::Ref<model::siege::FortressLocation>>& getFortress() const;

	const detail::LinkedMap<int32_t, runtime::Ref<model::siege::OutpostLocation>>& getOutpost() const;

	const detail::LinkedMap<int32_t, runtime::Ref<model::siege::SiegeLocation>>& getSiegeLocations() const;

	/** @return the agent location, nullptr (Java null) if there is none */
	runtime::Ptr<model::siege::AgentLocation> getAgentLoc() const;
};

} // namespace aion::gameserver::dataholders
