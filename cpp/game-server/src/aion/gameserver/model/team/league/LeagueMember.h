#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league {

/**
 * C++: K4 (a member of GeneralTeam.members), RefCounted and created with create(); the first TeamMember implementor of its tree, so it forwards
 * retain()/release() (hub-headers.md §9.2). Written with the M5g parties lane's stage-0 headers (m5g-plan.md I-02a, header request m5g-12)
 * because PlayerTeamCommandService's league arms name it; its bodies are accessors and ported with it.
 *
 * @author ATracer
 */
class LeagueMember : public runtime::RefCounted, public TeamMember {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<alliance::PlayerAlliance> alliance;
	runtime::Field<int32_t> leaguePosition;

protected:
	LeagueMember(alliance::PlayerAlliance& alliance, int32_t position);
	~LeagueMember() override;

public:
	static runtime::Ref<LeagueMember> create(alliance::PlayerAlliance& alliance, int32_t position);

	int32_t getObjectId() override;

	std::string getName() override;

	/** Java return type PlayerAlliance (the TeamMember<PlayerAlliance> binding) */
	runtime::Ptr<gameobjects::AionObject> getObject() override;

	/** Narrowing accessor: Java getObject() of TeamMember<PlayerAlliance> (defined in the .cpp: dereferencing the Ref needs the complete PlayerAlliance) */
	alliance::PlayerAlliance& getAlliance() const;

	void setLeaguePosition(int32_t value) { leaguePosition.set(value); }

	/** Java final */
	int32_t getLeaguePosition() const { return leaguePosition.get(); }

	void retain() const noexcept override { RefCounted::retain(); }
	void release() const noexcept override { RefCounted::release(); }
};

} // namespace aion::gameserver::model::team::league
