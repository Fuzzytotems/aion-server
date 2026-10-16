#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/PlayerTeamMember.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group {

/**
 * C++: K4 (a member of GeneralTeam.members), RefCounted through PlayerTeamMember and created with create() (m5g-plan.md GR-01, header request
 * m5g-7).
 *
 * @author ATracer
 */
class PlayerGroupMember : public PlayerTeamMember {
	AION_MAKE_REF_FRIEND
protected:
	explicit PlayerGroupMember(gameobjects::player::Player& player);
	~PlayerGroupMember() override;

public:
	static runtime::Ref<PlayerGroupMember> create(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::team::group
