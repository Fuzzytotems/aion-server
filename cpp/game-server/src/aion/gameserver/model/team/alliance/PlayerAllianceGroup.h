#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::alliance {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Java `TemporaryPlayerTeam<PlayerAllianceMember>`: the erased team (§8.1), so the
 * member parameters are TeamMember (bridge methods; the bodies cast to PlayerAllianceMember). The runtime base comes from TemporaryPlayerTeam
 * (RefCounted through AionObject). The group is held by Ref in PlayerAlliance.groups and Player.playerAllianceGroup, so it is no part of the
 * alliance: a player, task or packet may keep it after the alliance is disbanded. The alliance is therefore a retaining
 * `const Ref<PlayerAlliance>` (S0c freeze decision). The cycle PlayerAlliance.groups <-> PlayerAllianceGroup.alliance is cut by a cpp-breaker
 * (cycles.toml PlayerAlliance.groups; PlayerAllianceGroup.alliance is `accepted: cut elsewhere`): the port of PlayerAllianceService.disband
 * clears the alliance's groups after the AllianceDisbandEvent removed every member (C++ addition: Java leaves them to the garbage collector).
 * RefCounted: created with create() (PlayerAlliance's constructor: `groups.put(groupId, PlayerAllianceGroup::create(*this, groupId))`).
 *
 * @author ATracer
 */
class PlayerAllianceGroup : public TemporaryPlayerTeam {
	AION_MAKE_REF_FRIEND
private:
	// retaining: the group is held by players and PlayerAlliance.groups, so it is no part of the alliance (class comment)
	const runtime::Ref<PlayerAlliance> alliance;

protected:
	PlayerAllianceGroup(PlayerAlliance& alliance, int32_t objId);
	~PlayerAllianceGroup() override;

public:
	/** Java: new PlayerAllianceGroup(alliance, objId) */
	static runtime::Ref<PlayerAllianceGroup> create(PlayerAlliance& alliance, int32_t objId);

	/** Java PlayerAllianceMember (bridge: the erased GeneralTeam signature) */
	void addMember(TeamMember& member) override;

	/** Java PlayerAllianceMember (bridge: the erased GeneralTeam signature) */
	void onRemoveMember(TeamMember& member) override;

	int32_t getMaxMemberCount() override;

	int32_t getMinExpPlayerLevel() override;

	int32_t getMaxExpPlayerLevel() override;

	runtime::Ptr<PlayerAlliance> getAlliance() const { return alliance; }

	runtime::Ptr<common::legacy::LootGroupRules> getLootGroupRules() override;

	/** Narrowing accessors (Java TemporaryPlayerTeam<PlayerAllianceMember>, hub-headers.md §8.2): casts of the erased results */
	runtime::Ptr<PlayerAllianceMember> getMember(int32_t objectId);
	runtime::Ptr<PlayerAllianceMember> removeMember(TeamMember& member);
	runtime::Ptr<PlayerAllianceMember> removeMember(int32_t objectId);
	runtime::Ptr<PlayerAllianceMember> getLeader() const;
};

} // namespace aion::gameserver::model::team::alliance
