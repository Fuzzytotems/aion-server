#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/fwd.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/model/instance/playerreward/fwd.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic InstanceScore<T>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author xTz
 */
class InstanceScore : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<playerreward::InstancePlayerReward>> playerRewards{
		AION_LOCK_CLASS(InstanceScore::playerRewards#stripe)};
	runtime::Field<InstanceProgressionType> instanceProgressionType{InstanceProgressionType::START_PROGRESS};

protected:
	/** Java: the implicit default constructor */
	InstanceScore();

public:
	static runtime::Ref<InstanceScore> create();

	std::vector<runtime::Ptr<playerreward::InstancePlayerReward>> getPlayerRewards();

	bool containsPlayer(int32_t objectId);

	void removePlayerReward(playerreward::InstancePlayerReward& reward);

	runtime::Ptr<playerreward::InstancePlayerReward> getPlayerReward(int32_t objectId);

	void addPlayerReward(playerreward::InstancePlayerReward& reward);

	void setInstanceProgressionType(InstanceProgressionType value) { this->instanceProgressionType.set(value); }

	InstanceProgressionType getInstanceProgressionType() const { return this->instanceProgressionType.get(); }

	bool isRewarded();

	bool isReinforcing();

	bool isPreparing();

	bool isStartProgress();

	virtual void clear();

protected:
	~InstanceScore() override;
};

} // namespace aion::gameserver::model::instance::instancescore
