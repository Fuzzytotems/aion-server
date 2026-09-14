#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of PlayerGroup (`const std::unique_ptr<PlayerGroupStats>`), bound to the group
 * in the constructor. The package-private min/max level players are public fields (Java package access).
 *
 * @author ATracer
 */
class PlayerGroupStats : public runtime::OwnedPart {
private:
	runtime::OwnerRef<PlayerGroup> group;
	runtime::Field<int32_t> minExpPlayerLevel{};
	runtime::Field<int32_t> maxExpPlayerLevel{};

public:
	runtime::Field<runtime::Ref<gameobjects::player::Player>> minLevelPlayer{};
	runtime::Field<runtime::Ref<gameobjects::player::Player>> maxLevelPlayer{};

	/** Java package-private */
	explicit PlayerGroupStats(PlayerGroup& group);

	~PlayerGroupStats() override;

	void onAddPlayer(PlayerGroupMember& member);

	void onRemovePlayer(PlayerGroupMember& member);

private:
	void calculateExpLevels();

	void updateMinMaxLevelPlayers();

public:
	int32_t getMinExpPlayerLevel() const { return minExpPlayerLevel.get(); }

	int32_t getMaxExpPlayerLevel() const { return maxExpPlayerLevel.get(); }
};

} // namespace aion::gameserver::model::team::group
