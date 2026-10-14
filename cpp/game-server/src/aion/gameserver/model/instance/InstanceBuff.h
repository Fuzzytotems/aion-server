#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/templates/instance_bonusatrr/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::model::instance {

/**
 * The arenas' "boost morale" buff: the instance bonus attribute's penalty stats as stat functions of the player, ended by a task.
 * <p>
 * C++: the inner class InstanceBuffTask (fieldmap: captured this and the player) is replaced by the scheduled lambda of applyEffect, pinned
 * to this object, holding a Ref of the player; its run() is the lambda's endEffect(player).
 *
 * @author xTz
 */
class InstanceBuff : public runtime::RefCounted, public stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<stats::calc::functions::IStatFunction>> functions{AION_LOCK_CLASS(InstanceBuff::functions)};
	const templates::instance_bonusatrr::InstanceBonusAttr* instanceBonusAttr;
	runtime::Field<runtime::FutureRef> task{};
	runtime::Field<int64_t> endTime{};

protected:
	explicit InstanceBuff(int32_t buffId);
	~InstanceBuff() override;

public:
	static runtime::Ref<InstanceBuff> create(int32_t buffId);

	/** C++ only: Ref<StatOwner> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	void applyEffect(gameobjects::player::Player& player, int32_t time);

	void endEffect(gameobjects::player::Player& player);

	int32_t getRemainingTime();

	bool isActive();

private:
	void notify(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::instance
