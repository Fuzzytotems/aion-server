#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/league/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"

namespace aion::gameserver::model::team::league {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer
 */
class League : public GeneralTeam {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<common::legacy::LootGroupRules>> lootGroupRules{};

protected:
	explicit League(LeagueMember& leader);

public:
	static runtime::Ref<League> create(LeagueMember& value);

	std::vector<runtime::Ptr<gameobjects::player::Player>> getOnlineMembers() override;

	void addMember(TeamMember& member) override;

	void onRemoveMember(TeamMember& member) override;

	int32_t getMaxMemberCount() override;

	void sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets = {}) override;

	void sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
		std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets = {}) override;

	Race getRace() override;

	runtime::Ptr<gameobjects::player::Player> getCaptain();

	runtime::Ptr<common::legacy::LootGroupRules> getLootGroupRules() override { return this->lootGroupRules.get(); }

	void setLootGroupRules(runtime::Ptr<common::legacy::LootGroupRules> lootGroupRules);

	std::vector<runtime::Ptr<LeagueMember>> getSortedMembers();

	/** Reorganize alliances positions in league from 0 to size */
	runtime::Ptr<gameobjects::player::Player> reorganize();

	/** Search for player member in all alliances */
	runtime::Ptr<gameobjects::player::Player> getPlayerMember(int32_t playerObjId);

	void broadcast();

	void broadcast(gameobjects::player::Player& skippedPlayer);

	void broadcast(alliance::PlayerAlliance& skippedAlliance);

	void broadcast(runtime::Ptr<alliance::PlayerAlliance> skippedAlliance, runtime::Ptr<gameobjects::player::Player> skippedPlayer);

	std::vector<runtime::Ptr<gameobjects::player::Player>> getCaptains();

protected:
	~League() override;

public:
	/** Narrowing accessors (Java GeneralTeam<PlayerAlliance, LeagueMember>, hub-headers.md §8.2): casts of the erased results */
	runtime::Ptr<LeagueMember> getMember(int32_t objectId);
	runtime::Ptr<LeagueMember> removeMember(TeamMember& member);
	runtime::Ptr<LeagueMember> removeMember(int32_t objectId);
	runtime::Ptr<LeagueMember> getLeader() const;

	/** Narrowing accessor (Java GeneralTeam<PlayerAlliance, LeagueMember>.getLeaderObject(), hub-headers.md §8.2) */
	runtime::Ptr<alliance::PlayerAlliance> getLeaderObject();
};

} // namespace aion::gameserver::model::team::league
