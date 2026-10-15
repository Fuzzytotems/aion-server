#pragma once

#include <cstdint>
#include <vector>

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

	std::vector<runtime::Ref<model::siege::Assaulter>> computeWave();

	/** Java addAssaulters(List output, List input, float budget): input is null (nullptr) for a type the EnumMap has no list for */
	void addAssaulters(std::vector<runtime::Ref<model::siege::Assaulter>>& output, const std::vector<runtime::Ref<model::siege::Assaulter>>* input,
		float budget);

	void announce(network::aion::serverpackets::SM_SYSTEM_MESSAGE msg);

	void calculateDifficultySettings();

	float getFactionBalanceMultiplier();

	float getInfluenceMultiplier();

public:
	void onDredgionCommanderKilled();

	/** C++ additions for tests: Java's private difficulty settings */
	float getDifficultyForTests() const { return difficulty.get(); }
	float getSpawnBudgetForTests() const { return spawnBudget.get(); }
	int32_t getMinSpawnDelayForTests() const { return minSpawnDelay.get(); }
	int32_t getPossibleCommanderCountForTests() const { return possibleCommanderCount.get(); }
	int32_t getWaveCountForTests() const { return waveCount.get(); }

protected:
	~FortressAssault() override;
};

} // namespace aion::gameserver::services::siege
