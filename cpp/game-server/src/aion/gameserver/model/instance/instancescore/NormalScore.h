#pragma once

#include <cstdint>

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * Java NormalScore extends InstanceScore&lt;InstancePlayerReward&gt;: the instance-wide counters its score writer sends.
 *
 * @author Cheatkiller
 */
class NormalScore : public InstanceScore {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> points{};
	runtime::Field<int32_t> basicAp{};
	runtime::Field<int32_t> finalAp{};
	runtime::Field<int32_t> rank{7};
	runtime::Field<int32_t> rewardItem1{};
	runtime::Field<int32_t> rewardItem2{};
	runtime::Field<int32_t> rewardItem3{};
	runtime::Field<int32_t> rewardItem4{};
	runtime::Field<int32_t> rewardItem1Count{};
	runtime::Field<int32_t> rewardItem2Count{};
	runtime::Field<int32_t> rewardItem3Count{};
	runtime::Field<int32_t> rewardItem4Count{};

protected:
	NormalScore();
	~NormalScore() override;

public:
	static runtime::Ref<NormalScore> create();

	void addPoints(int32_t value);

	int32_t getPoints() const { return points.get(); }

	void setPoints(int32_t value) { points.set(value); }

	void setRank(int32_t value) { rank.set(value); }

	int32_t getRank() const { return rank.get(); }

	int32_t getBasicAp() const { return basicAp.get(); }

	void setBasicAp(int32_t value) { basicAp.set(value); }

	int32_t getFinalAp() const { return finalAp.get(); }

	void setFinalAp(int32_t value) { finalAp.set(value); }

	int32_t getRewardItem1() const { return rewardItem1.get(); }

	void setRewardItem1(int32_t value) { rewardItem1.set(value); }

	int32_t getRewardItem2() const { return rewardItem2.get(); }

	void setRewardItem2(int32_t value) { rewardItem2.set(value); }

	int32_t getRewardItem3() const { return rewardItem3.get(); }

	void setRewardItem3(int32_t value) { rewardItem3.set(value); }

	int32_t getRewardItem4() const { return rewardItem4.get(); }

	void setRewardItem4(int32_t value) { rewardItem4.set(value); }

	int32_t getRewardItem1Count() const { return rewardItem1Count.get(); }

	void setRewardItem1Count(int32_t value) { rewardItem1Count.set(value); }

	int32_t getRewardItem2Count() const { return rewardItem2Count.get(); }

	void setRewardItem2Count(int32_t value) { rewardItem2Count.set(value); }

	int32_t getRewardItem3Count() const { return rewardItem3Count.get(); }

	void setRewardItem3Count(int32_t value) { rewardItem3Count.set(value); }

	int32_t getRewardItem4Count() const { return rewardItem4Count.get(); }

	void setRewardItem4Count(int32_t value) { rewardItem4Count.set(value); }
};

} // namespace aion::gameserver::model::instance::instancescore
