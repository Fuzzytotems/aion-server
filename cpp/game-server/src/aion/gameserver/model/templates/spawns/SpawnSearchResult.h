#pragma once

#include <cstdint>
#include <utility>

#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::templates::spawns {

/**
 * A search result of the spawn data (the world id and the spot of a spawn): a value class (fieldmap K5).
 * <p>
 * C++: the result owns its spot by value. Java's only construction site (SpawnsData.toSpawnSearchResult) wraps a new SpawnSpotTemplate built
 * from the current coordinates of a run-time SpawnTemplate, which nothing but the result references; a pointer into the bound spawn data would
 * carry the load-time coordinates instead. The spot is not static data: do not capture the address of getSpot() in scheduled work (the Pin
 * rules treat SpawnSpotTemplate pointers as static templates).
 *
 * @author Rolandas
 */
class SpawnSearchResult final {
private:
	// fieldmap: owned value, not the proposed `const SpawnSpotTemplate*`: Java's `new SpawnSpotTemplate(...)` is held only by the result
	SpawnSpotTemplate spot;
	int32_t worldId;

public:
	SpawnSearchResult(int32_t worldIdValue, SpawnSpotTemplate spotValue) : spot(std::move(spotValue)), worldId(worldIdValue) {}

	const SpawnSpotTemplate& getSpot() const { return spot; }

	int32_t getWorldId() const { return worldId; }
};

} // namespace aion::gameserver::model::templates::spawns
