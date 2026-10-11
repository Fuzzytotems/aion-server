#pragma once

#include <cstdint>

#include "aion/gameserver/model/autogroup/AutoInstance.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::model::autogroup {

/**
 * Includes Dredgion, Engulfed Ophidan Bridge, Idgel Dome, Iron Wall Warfront and Kamar Battlefield.
 * <p>
 * Java: model/autogroup/AutoPvpInstance.java. Its `synchronized addLookingForParty` locks the object's Monitor (SYNCHRONIZED(*this)).
 *
 * @author Estrayl
 */
class AutoPvpInstance : public AutoInstance {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<AutoPvpInstance> create(AutoGroupType agt);

	AGQuestion addLookingForParty(LookingForParty& lookingForParty) override;

	void onEnterInstance(gameobjects::player::Player& player) override;

	void onPressEnter(gameobjects::player::Player& player) override;

	void onLeaveInstance(gameobjects::player::Player& player) override;

	int32_t getMaxPlayers() override;

protected:
	explicit AutoPvpInstance(AutoGroupType agt);
	~AutoPvpInstance() override;

private:
	int32_t getMaxPlayers(Race race);
};

} // namespace aion::gameserver::model::autogroup
