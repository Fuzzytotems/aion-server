#pragma once

#include <cstdint>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * Java generic PvpInstanceScore&lt;T extends InstancePlayerReward&gt;: erased to a non-template class like its base (hub-headers.md §8.1).
 * The synchronized methods lock the object's Monitor (SYNCHRONIZED(*this)).
 */
class PvpInstanceScore : public InstanceScore {
	AION_MAKE_REF_FRIEND
private:
	const int32_t winnerApReward;
	const int32_t loserApReward;
	const int32_t drawApReward;
	runtime::Field<int32_t> asmodiansPoints{};
	runtime::Field<int32_t> elyosPoints{};
	runtime::Field<int32_t> asmodiansKills{};
	runtime::Field<int32_t> elyosKills{};

protected:
	PvpInstanceScore(int32_t winnerApReward, int32_t loserApReward, int32_t drawApReward);
	~PvpInstanceScore() override;

public:
	static runtime::Ref<PvpInstanceScore> create(int32_t winnerApReward, int32_t loserApReward, int32_t drawApReward);

	int32_t getWinnerApReward() const { return winnerApReward; }

	int32_t getLoserApReward() const { return loserApReward; }

	int32_t getDrawApReward() const { return drawApReward; }

	int32_t getAsmodiansPoints() const { return asmodiansPoints.get(); }

	int32_t getElyosPoints() const { return elyosPoints.get(); }

	int32_t getPointsByRace(Race race) const;

	int32_t getAsmodiansKills() const { return asmodiansKills.get(); }

	int32_t getElyosKills() const { return elyosKills.get(); }

	Race getRaceWithHighestPoints() const;

	void addPointsByRace(Race race, int32_t points); // synchronized

	void incrementKillsByRace(Race race); // synchronized
};

} // namespace aion::gameserver::model::instance::instancescore
