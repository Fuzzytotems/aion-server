#pragma once

#include <cstdint>
#include <functional>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::services::panesterra::ahserion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Yeats, Estrayl
 */
class PanesterraTeam : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** Java static final WorldPosition (defined in PanesterraTeam.cpp: a never destroyed Ref, like ItemService::DEFAULT_UPDATE_PREDICATE) */
	// fieldmap: static final object (hub-headers.md §11.1) defined in the .cpp as a reference to a never destroyed Ref
	static const runtime::Ref<world::WorldPosition>& ELYOS_ORIGIN_POS;
	// fieldmap: static final object (hub-headers.md §11.1) defined in the .cpp as a reference to a never destroyed Ref
	static const runtime::Ref<world::WorldPosition>& ASMO_ORIGIN_POS;
	runtime::ArrayList<int32_t> teamMembers{AION_LOCK_CLASS(PanesterraTeam::teamMembers)};
	const PanesterraFaction faction;
	const runtime::Ref<world::WorldPosition> originPosition;
	const runtime::Ref<world::WorldPosition> startPosition;
	runtime::Field<bool> isEliminated_{};

protected:
	explicit PanesterraTeam(PanesterraFaction faction);

public:
	static runtime::Ref<PanesterraTeam> create(PanesterraFaction value);

	void moveTeamMembersToOriginPosition();

	void forEachMember(const std::function<void(model::gameobjects::player::Player&)>& consumer);

	void movePlayerToOriginPosition(model::gameobjects::player::Player& player);

	void movePlayerToStartPosition(model::gameobjects::player::Player& player);

	void addTeamMemberIfAbsent(int32_t playerId);

	bool isTeamMember(int32_t playerId);

	void removeTeamMember(int32_t playerId);

	bool isEliminated() const { return this->isEliminated_.get(); }

	void setIsEliminated(bool value) { this->isEliminated_.set(value); }

	runtime::Ptr<world::WorldPosition> getStartPosition() const { return this->startPosition; }

	int32_t getMemberCount();

	PanesterraFaction getFaction() const { return this->faction; }

protected:
	~PanesterraTeam() override;
};

} // namespace aion::gameserver::services::panesterra::ahserion
