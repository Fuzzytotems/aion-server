#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"

namespace aion::gameserver::model::siege {

/**
 * These bosses only appear when a faction conquers the balaurea fortresses of their enemy map.
 * If Elyos conquer Gelkmaros' fortresses Enraged Mastarius will appear on Ancient City of Marayas.
 * If Asmodians conquer Inggison's fortresses Enraged Veille will appear on Inggison Outpost.
 * He/She will stay for about 2 hours after that he/she disappears and re-spawns after the end of the next siege if conditions
 * are still met.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Source
 */
class OutpostLocation : public SiegeLocation {
	AION_MAKE_REF_FRIEND
protected:
	explicit OutpostLocation(const templates::siegelocation::SiegeLocationTemplate* template_);

public:
	static runtime::Ref<OutpostLocation> create(const templates::siegelocation::SiegeLocationTemplate* value);

	int32_t getNextState() override;

	std::vector<int32_t> getFortressDependency();

	/**
	 * Shouldn't be necessary anymore, but re-check packets first before removing this.
	 * Silentera entrances do not depend on fortresses since 4.x.
	 */
	bool isSilenteraAllowed();

protected:
	~OutpostLocation() override;
};

} // namespace aion::gameserver::model::siege
