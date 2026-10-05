#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/PlayerTeamMember.h"
#include "aion/gameserver/model/team/alliance/fwd.h"

namespace aion::gameserver::model::team::alliance {

/**
 * C++: K4 (a member of GeneralTeam.members), RefCounted through PlayerTeamMember and created with create(). Written with the M5g parties lane's
 * stage-0 headers (m5g-plan.md I-02a, header request m5g-11) because the party packets and SM_ALLIANCE_MEMBER_INFO name it; its five bodies are
 * accessors and ported with it.
 *
 * @author ATracer
 */
class PlayerAllianceMember : public PlayerTeamMember {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> allianceId;

protected:
	explicit PlayerAllianceMember(gameobjects::player::Player& player);
	~PlayerAllianceMember() override;

public:
	static runtime::Ref<PlayerAllianceMember> create(gameobjects::player::Player& player);

	int32_t getAllianceId() const { return allianceId.get(); }

	void setAllianceId(int32_t value) { allianceId.set(value); }

	/** Java final */
	runtime::Ptr<PlayerAllianceGroup> getPlayerAllianceGroup();

	/** Java final */
	void setPlayerAllianceGroup(runtime::Ptr<PlayerAllianceGroup> playerAllianceGroup);
};

} // namespace aion::gameserver::model::team::alliance
