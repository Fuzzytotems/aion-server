#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"

namespace aion::gameserver::model::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Estrayl
 */
class AgentLocation : public SiegeLocation {
	AION_MAKE_REF_FRIEND
protected:
	explicit AgentLocation(const templates::siegelocation::SiegeLocationTemplate* template_);

public:
	static runtime::Ref<AgentLocation> create(const templates::siegelocation::SiegeLocationTemplate* value);

	int32_t getNextState() override;

	SiegeRace getRace() override;

protected:
	~AgentLocation() override;
};

} // namespace aion::gameserver::model::siege
