#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/siege/Assault.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Luzien, Estrayl
 *         TODO: Fortress gate, gate restoration stone and aetheric field destruction
 */
class FortressAssault : public Assault {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ArrayList<>()
	runtime::ArrayList<runtime::Ref<model::siege::Assaulter>> commanderSpawnList{AION_LOCK_CLASS(FortressAssault::commanderSpawnList)};
	const model::templates::siegelocation::AssaultData* assaultData;
	runtime::Field<float> difficulty{};
	runtime::Field<float> commanderSpawnChance{};
	runtime::Field<float> spawnBudget{};
	runtime::Field<float> startBudget{};
	runtime::Field<int32_t> minSpawnDelay{};
	runtime::Field<int32_t> waveCount{};
	runtime::Field<int32_t> possibleCommanderCount{};

protected:
	explicit FortressAssault(FortressSiege& siege);

public:
	static runtime::Ref<FortressAssault> create(FortressSiege& siege);

protected:
	void handleAssault() override;

	void onAssaultFinish(bool isCaptured) override;

private:
	void scheduleSpawns();

	void spawnWave();

	std::vector<runtime::Ptr<model::siege::Assaulter>> computeWave();

	void addAssaulters(const std::vector<runtime::Ptr<model::siege::Assaulter>>& output,
		const std::vector<runtime::Ptr<model::siege::Assaulter>>& input,
		float budget);

	void announce(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg);

	void calculateDifficultySettings();

	float getFactionBalanceMultiplier();

	float getInfluenceMultiplier();

public:
	void onDredgionCommanderKilled();

protected:
	~FortressAssault() override;
};

} // namespace aion::gameserver::services::siege
