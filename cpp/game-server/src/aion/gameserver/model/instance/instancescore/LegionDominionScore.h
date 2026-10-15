#pragma once

#include <cstdint>

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * Java LegionDominionScore extends InstanceScore&lt;InstancePlayerReward&gt;: the instance-wide counters its score writer sends.
 *
 * @author Yeats
 */
class LegionDominionScore : public InstanceScore {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> points{};
	runtime::Field<int32_t> finalGP{};
	runtime::Field<int32_t> rank{7};
	runtime::Field<int32_t> finalAP{};
	runtime::Field<int32_t> rewardItem1{};
	runtime::Field<int32_t> rewardItem2{};
	runtime::Field<int32_t> rewardItem3{};
	runtime::Field<int32_t> rewardItem4{};
	runtime::Field<int32_t> rewardItem1Count{};
	runtime::Field<int32_t> rewardItem2Count{};
	runtime::Field<int32_t> rewardItem3Count{};
	runtime::Field<int32_t> rewardItem4Count{};

protected:
	LegionDominionScore();
	~LegionDominionScore() override;

public:
	static runtime::Ref<LegionDominionScore> create();

	void addPoints(int32_t value);

	int32_t getPoints() const { return points.get(); }

	int32_t getFinalGP() const { return finalGP.get(); }

	int32_t getRank() const { return rank.get(); }

	int32_t getFinalAp() const { return finalAP.get(); }

	int32_t getRewardItem1() const { return rewardItem1.get(); }

	int32_t getRewardItem2() const { return rewardItem2.get(); }

	int32_t getRewardItem3() const { return rewardItem3.get(); }

	int32_t getRewardItem4() const { return rewardItem4.get(); }

	int32_t getRewardItem1Count() const { return rewardItem1Count.get(); }

	int32_t getRewardItem2Count() const { return rewardItem2Count.get(); }

	int32_t getRewardItem3Count() const { return rewardItem3Count.get(); }

	int32_t getRewardItem4Count() const { return rewardItem4Count.get(); }

	void setPoints(int32_t value) { points.set(value); }

	void setFinalGP(int32_t value) { finalGP.set(value); }

	void setRank(int32_t value) { rank.set(value); }

	void setFinalAP(int32_t value) { finalAP.set(value); }

	void setRewardItem1(int32_t value) { rewardItem1.set(value); }

	void setRewardItem2(int32_t value) { rewardItem2.set(value); }

	void setRewardItem3(int32_t value) { rewardItem3.set(value); }

	void setRewardItem4(int32_t value) { rewardItem4.set(value); }

	void setRewardItem1Count(int32_t value) { rewardItem1Count.set(value); }

	void setRewardItem2Count(int32_t value) { rewardItem2Count.set(value); }

	void setRewardItem3Count(int32_t value) { rewardItem3Count.set(value); }

	void setRewardItem4Count(int32_t value) { rewardItem4Count.set(value); }
};

} // namespace aion::gameserver::model::instance::instancescore
