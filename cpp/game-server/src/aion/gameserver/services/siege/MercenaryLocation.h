#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"
#include "aion/gameserver/model/templates/spawns/mercenaries/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.MercenaryLocation - the mercenaries a fortress owner can hire for one zone during a siege (FortressSiege keeps one per
 * SiegeMercenaryZone of its fortress while it runs; MercenaryAI asks it).
 *
 * @author Whoop
 */
class MercenaryLocation : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<model::gameobjects::VisibleObject>> spawnedMercs{AION_LOCK_CLASS(MercenaryLocation::spawnedMercs)};
	const model::templates::spawns::mercenaries::MercenaryZone* spawns = nullptr; // TODO (Java): Change this to SpawnGroup
	const model::templates::siegelocation::SiegeMercenaryZone* smz;
	/** Java: Race.getRaceByString(race.name()) - null for the BALAUR siege race (Race has no such constant) */
	const std::optional<model::Race> race;
	runtime::Field<int64_t> lastSpawn{};
	const int32_t siegeId;

protected:
	MercenaryLocation(const model::templates::siegelocation::SiegeMercenaryZone* templateValue, model::siege::SiegeRace raceValue, int32_t siegeIdValue);

public:
	static runtime::Ref<MercenaryLocation> create(const model::templates::siegelocation::SiegeMercenaryZone* templateValue, model::siege::SiegeRace raceValue,
		int32_t siegeIdValue);

	void spawn();

	void despawnCurrentMercs();

	/** @return true if cooldown is expired and enough mercs are dead */
	bool isRequestValid();

private:
	/** Check if enough mercs are still alive. @return false if 50% are alive */
	bool hasEnoughMercsAlive();

	const model::templates::spawns::mercenaries::MercenaryZone* getSpawnZone();

public:
	int32_t getCosts();

	int32_t getMsgId();

protected:
	~MercenaryLocation() override;
};

} // namespace aion::gameserver::services::siege
