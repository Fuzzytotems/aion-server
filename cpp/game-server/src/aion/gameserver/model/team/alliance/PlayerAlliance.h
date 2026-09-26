#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::alliance {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer
 */
class PlayerAlliance : public TemporaryPlayerTeam {
	AION_MAKE_REF_FRIEND
private:
	// PlayerAllianceGroup is RefCounted through TemporaryPlayerTeam (players hold it by Ref): Ref values, no PartMap (cycles.toml cpp-breaker)
	runtime::HashMap<int32_t, runtime::Ref<PlayerAllianceGroup>> groups{AION_LOCK_CLASS(PlayerAlliance::groups)};
	runtime::CopyOnWriteArrayList<int32_t> viceCaptainIds{AION_LOCK_CLASS(PlayerAlliance::viceCaptainIds)}; // Java: = new CopyOnWriteArrayList<>()
	runtime::Field<int32_t> allianceReadyStatus{};
	const TeamType type;
	runtime::Field<runtime::Ref<team::league::League>> league{};

protected:
	PlayerAlliance(PlayerAllianceMember& leader, TeamType type);

public:
	static runtime::Ref<PlayerAlliance> create(PlayerAllianceMember& value, TeamType typeValue);

	/** @param member a PlayerAllianceMember (Java TM erased to TeamMember, hub-headers.md §8.2) */
	void addMember(TeamMember& member) override;

	/** @param member a PlayerAllianceMember */
	void onRemoveMember(TeamMember& member) override;

	int32_t getMaxMemberCount() override;

	int32_t getMinExpPlayerLevel() override;

	int32_t getMaxExpPlayerLevel() override;

	runtime::Ptr<PlayerAllianceGroup> getOpenAllianceGroup();

	runtime::Ptr<PlayerAllianceGroup> getAllianceGroup(std::optional<int32_t> allianceGroupId);

	runtime::CopyOnWriteArrayList<int32_t>& getViceCaptainIds() { return this->viceCaptainIds; }

	bool isViceCaptain(gameobjects::player::Player& player);

	bool isSomeCaptain(gameobjects::player::Player& player);

	int32_t getAllianceReadyStatus() const { return this->allianceReadyStatus.get(); }

	void setAllianceReadyStatus(int32_t value) { this->allianceReadyStatus.set(value); }

	runtime::Ptr<team::league::League> getLeague() const { return this->league.get(); }

	void setLeague(runtime::Ptr<team::league::League> league);

	bool isInLeague();

	int32_t groupSize();

	std::vector<runtime::Ptr<PlayerAllianceGroup>> getGroups();

	TeamType getTeamType() const { return this->type; }

	runtime::Ptr<common::legacy::LootGroupRules> getLootGroupRules() override;

protected:
	~PlayerAlliance() override;

public:
	/** Narrowing accessors (Java TemporaryPlayerTeam<PlayerAllianceMember>, hub-headers.md §8.2): casts of the erased results */
	runtime::Ptr<PlayerAllianceMember> getMember(int32_t objectId);
	runtime::Ptr<PlayerAllianceMember> removeMember(TeamMember& member);
	runtime::Ptr<PlayerAllianceMember> removeMember(int32_t objectId);
	runtime::Ptr<PlayerAllianceMember> getLeader() const;
};

} // namespace aion::gameserver::model::team::alliance
