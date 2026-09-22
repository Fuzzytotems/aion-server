#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/ai/follow/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::ai::follow {

/**
 * The periodic task that keeps a summon near the creature it follows: it releases the summon when its master ran away, re-validates the move
 * when the target moved, and attacks the target once the summon arrived.
 * <p>
 * RefCounted (fieldmap K4: FollowStartService::newFollowingToTargetCheckTask schedules it at a fixed rate, so it reads target, summon and
 * master on the pool thread in every run and retains all three, as Java's scheduled Runnable does). Java `new FollowSummonTaskAI(target,
 * summon)` is `FollowSummonTaskAI::create(target, summon)`.
 *
 * @author xTz
 */
class FollowSummonTaskAI : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::Creature> target;
	const runtime::Ref<model::gameobjects::Summon> summon;
	const runtime::Ref<model::gameobjects::player::Player> master;
	runtime::Field<float> targetX{};
	runtime::Field<float> targetY{};
	runtime::Field<float> targetZ{};

protected:
	FollowSummonTaskAI(model::gameobjects::Creature& target, model::gameobjects::Summon& summon);
	~FollowSummonTaskAI() override;

public:
	/** Java: new FollowSummonTaskAI(target, summon) */
	static runtime::Ref<FollowSummonTaskAI> create(model::gameobjects::Creature& target, model::gameobjects::Summon& summon);

private:
	void setLeadingCoordinates();

public:
	void run(); // @Override of a Java library type

private:
	bool isInTargetRange();

	bool isInMasterRange();

protected:
	virtual void onDestination();

private:
	void onOutOfTargetRange();
};

} // namespace aion::gameserver::ai::follow
