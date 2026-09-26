#include "aion/gameserver/dataholders/SiegeLocationData.h"

#include "aion/gameserver/model/siege/AgentLocation.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/OutpostLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/siege/SiegeType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dataholders {

using model::siege::AgentLocation;
using model::siege::ArtifactLocation;
using model::siege::FortressLocation;
using model::siege::OutpostLocation;
using model::siege::SiegeLocation;
using model::siege::SiegeRace;
using model::siege::SiegeType;

SiegeLocationData::SiegeLocationData() = default;

SiegeLocationData::~SiegeLocationData() = default;

void SiegeLocationData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// the hook creates RefCounted run-time objects (and SpawnsData writes collection shims): a nested TaskScope when the load runs in one
	// (DataManager), an own one when a holder is bound alone (tests binding templates outside a scope)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	artifactLocations.clear();
	fortressLocations.clear();
	outpostLocations.clear();
	siegeLocations.clear();
	for (const model::templates::siegelocation::SiegeLocationTemplate& template_ : siegeLocationTemplates) {
		if (!template_.getType())
			throw runtime::NullPointerException("Cannot invoke \"SiegeType.ordinal()\" because \"template.getType()\" is null");
		switch (*template_.getType()) {
			case SiegeType::FORTRESS: {
				runtime::Ref<FortressLocation> fortress = FortressLocation::create(&template_);
				fortressLocations.put(template_.getId(), fortress);
				siegeLocations.put(template_.getId(), fortress);
				artifactLocations.put(template_.getId(), ArtifactLocation::create(&template_));
				break;
			}
			case SiegeType::ARTIFACT: {
				runtime::Ref<ArtifactLocation> artifact = ArtifactLocation::create(&template_);
				artifactLocations.put(template_.getId(), artifact);
				siegeLocations.put(template_.getId(), artifact);
				break;
			}
			case SiegeType::OUTPOST: {
				runtime::Ref<OutpostLocation> outpost = OutpostLocation::create(&template_);
				// Java: outpost.getLocationId(), which is template.getId()
				if (template_.getId() == 2111)
					outpost->setRace(SiegeRace::ELYOS);
				else if (template_.getId() == 3111)
					outpost->setRace(SiegeRace::ASMODIANS);
				outpostLocations.put(template_.getId(), outpost);
				siegeLocations.put(template_.getId(), outpost);
				break;
			}
			case SiegeType::AGENT_FIGHT: {
				agentLoc = AgentLocation::create(&template_);
				siegeLocations.put(template_.getId(), agentLoc);
				break;
			}
			default:
				break;
		}
	}
}

int32_t SiegeLocationData::size() const {
	return static_cast<int32_t>(siegeLocations.size());
}

const detail::LinkedMap<int32_t, runtime::Ref<ArtifactLocation>>& SiegeLocationData::getArtifacts() const {
	return artifactLocations;
}

const detail::LinkedMap<int32_t, runtime::Ref<FortressLocation>>& SiegeLocationData::getFortress() const {
	return fortressLocations;
}

const detail::LinkedMap<int32_t, runtime::Ref<OutpostLocation>>& SiegeLocationData::getOutpost() const {
	return outpostLocations;
}

const detail::LinkedMap<int32_t, runtime::Ref<SiegeLocation>>& SiegeLocationData::getSiegeLocations() const {
	return siegeLocations;
}

runtime::Ptr<AgentLocation> SiegeLocationData::getAgentLoc() const {
	return agentLoc;
}

} // namespace aion::gameserver::dataholders
