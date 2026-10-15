#pragma once

#include "aion/gameserver/model/autogroup/AutoInstance.h"

namespace aion::gameserver::model::autogroup {

/**
 * The free-for-all and solo arenas (Arena of Chaos, of Discipline, of Glory and their training grounds).
 * <p>
 * Java: model/autogroup/AutoPvPFFAInstance.java. Its `synchronized addLookingForParty` locks the object's Monitor (SYNCHRONIZED(*this)). The
 * casts of the instance's score to PvPArenaScore are runtime::cast (Java's ClassCastException).
 *
 * @author xTz
 */
class AutoPvPFFAInstance : public AutoInstance {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<AutoPvPFFAInstance> create(AutoGroupType agt);

	void onInstanceCreate(world::WorldMapInstance& instance) override;

	AGQuestion addLookingForParty(LookingForParty& lookingForParty) override;

	void onPressEnter(gameobjects::player::Player& player) override;

	void onLeaveInstance(gameobjects::player::Player& player) override;

protected:
	explicit AutoPvPFFAInstance(AutoGroupType agt);
	~AutoPvPFFAInstance() override;
};

} // namespace aion::gameserver::model::autogroup
