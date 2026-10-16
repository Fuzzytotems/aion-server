#pragma once

#include <cstdint>

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * Java DarkPoetaScore extends InstanceScore&lt;InstancePlayerReward&gt;: the instance-wide counters its score writer sends.
 *
 * @author xTz
 */
class DarkPoetaScore : public InstanceScore {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> points{};
	runtime::Field<int32_t> npcKills{};
	runtime::Field<int32_t> rank{7};
	runtime::Field<int32_t> collections{};

protected:
	DarkPoetaScore();
	~DarkPoetaScore() override;

public:
	static runtime::Ref<DarkPoetaScore> create();

	void addPoints(int32_t value);

	int32_t getPoints() const { return points.get(); }

	void addNpcKill() { npcKills.set(npcKills.get() + 1); }

	int32_t getNpcKills() const { return npcKills.get(); }

	void setRank(int32_t value) { rank.set(value); }

	int32_t getRank() const { return rank.get(); }

	void addGather() { collections.set(collections.get() + 1); }

	int32_t getGatherCollections() const { return collections.get(); }
};

} // namespace aion::gameserver::model::instance::instancescore
