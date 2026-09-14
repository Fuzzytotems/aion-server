#pragma once

#include <cstdint>
#include <memory>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Java `TemporaryPlayerTeam<PlayerGroupMember>`: the erased team (§8.1), so the member
 * parameters are TeamMember (bridge methods; the bodies cast to PlayerGroupMember). RefCounted (through AionObject), created with create(); the
 * group stats are a part created by the constructor. The constructor ends with setLeader(leader), which needs the complete PlayerGroupMember
 * (not written yet) and an unported GeneralTeam body, so it stays `AION_UNPORTED` at that statement.
 *
 * @author ATracer
 */
class PlayerGroup : public TemporaryPlayerTeam {
	AION_MAKE_REF_FRIEND
private:
	const std::unique_ptr<PlayerGroupStats> playerGroupStats;
	const TeamType type;

protected:
	PlayerGroup(PlayerGroupMember& leader, TeamType type, int32_t id);
	~PlayerGroup() override;

public:
	/** Java: new PlayerGroup(leader, type, id) (id 0: a new id from IDFactory, auto-released) */
	static runtime::Ref<PlayerGroup> create(PlayerGroupMember& leader, TeamType type, int32_t id);

	/** Java PlayerGroupMember (bridge: the erased GeneralTeam signature) */
	void addMember(TeamMember& member) override;

	/** Java PlayerGroupMember (bridge: the erased GeneralTeam signature) */
	void onRemoveMember(TeamMember& member) override;

	int32_t getMaxMemberCount() override;

	int32_t getMinExpPlayerLevel() override;

	int32_t getMaxExpPlayerLevel() override;

	TeamType getTeamType() const { return type; }

	/** Narrowing accessors (Java TemporaryPlayerTeam<PlayerGroupMember>, hub-headers.md §8.2): casts of the erased results */
	runtime::Ptr<PlayerGroupMember> getMember(int32_t objectId);
	runtime::Ptr<PlayerGroupMember> removeMember(TeamMember& member);
	runtime::Ptr<PlayerGroupMember> removeMember(int32_t objectId);
	runtime::Ptr<PlayerGroupMember> getLeader() const;
};

} // namespace aion::gameserver::model::team::group
