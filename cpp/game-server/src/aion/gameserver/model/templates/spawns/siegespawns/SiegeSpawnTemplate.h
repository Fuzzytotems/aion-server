#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/fwd.h"

namespace aion::gameserver::model::templates::spawns::siegespawns {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5; closure of SiegeNpc's constructor). An OwnedPart of its SpawnGroup like SpawnTemplate:
 * public constructors taking the group (S0B-117). Java `new SiegeSpawnTemplate(..., spawnGroup, x, y, z, ...)` delegates to the SpawnTemplate
 * constructor that ends with addTemplate(): `create(...)` constructs the part and moves it into the group (SpawnTemplate::addTemplate). The spot
 * constructor does not add itself (SpawnGroup's constructors do).
 *
 * @author xTz
 */
class SiegeSpawnTemplate : public SpawnTemplate {
private:
	const int32_t siegeId;
	const model::siege::SiegeRace siegeRace;
	const model::siege::SiegeModType siegeModType;

public:
	/** Java: new SiegeSpawnTemplate(siegeId, siegeRace, siegeModType, spawnGroup, spot) (not added to the group) */
	SiegeSpawnTemplate(int32_t siegeId, model::siege::SiegeRace siegeRace, model::siege::SiegeModType siegeModType, SpawnGroup& spawnGroup,
		const SpawnSpotTemplate* spot);

	/** Does not add itself to the group (Java: addTemplate() in the delegated constructor; C++: create does) */
	SiegeSpawnTemplate(int32_t siegeId, model::siege::SiegeRace siegeRace, model::siege::SiegeModType siegeModType, SpawnGroup& spawnGroup, float x,
		float y, float z, int8_t heading, int32_t randWalk, std::optional<std::string_view> walkerId, int32_t staticId);

	~SiegeSpawnTemplate() override;

	/** Java `new SiegeSpawnTemplate(siegeId, siegeRace, siegeModType, spawnGroup, x, y, z, heading, randWalk, walkerId, staticId)`: constructs, then
	 * adds the template to its group */
	static runtime::Ref<SiegeSpawnTemplate> create(int32_t siegeId, model::siege::SiegeRace siegeRace, model::siege::SiegeModType siegeModType,
		SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk, std::optional<std::string_view> walkerId, int32_t staticId);

	int32_t getSiegeId() const { return siegeId; }

	model::siege::SiegeRace getSiegeRace() const { return siegeRace; }

	model::siege::SiegeModType getSiegeModType() const { return siegeModType; }
};

} // namespace aion::gameserver::model::templates::spawns::siegespawns
