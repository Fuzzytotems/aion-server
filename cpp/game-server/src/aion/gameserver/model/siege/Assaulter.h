#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/fwd.h"

namespace aion::gameserver::model::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Estrayl
 */
class Assaulter : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t npcId;
	const float spawnCost;
	const int32_t headingOffset;
	const int32_t distanceOffset;

protected:
	Assaulter(int32_t npcId, float spawnCost, int32_t headingOffset, int32_t distanceOffset);

public:
	static runtime::Ref<Assaulter> create(int32_t value, float spawnCostValue, int32_t headingOffsetValue, int32_t distanceOffsetValue);

	int32_t getNpcId() const { return this->npcId; }

	float getSpawnCost() const { return this->spawnCost; }

	int32_t getHeadingOffset() const { return this->headingOffset; }

	int32_t getDistanceOffset() const { return this->distanceOffset; }

protected:
	~Assaulter() override;
};

} // namespace aion::gameserver::model::siege
