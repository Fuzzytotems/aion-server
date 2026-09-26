#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team {

/**
 * C++: Java `TeamMember<M>` is one non-template interface with `M` spelled AionObject (docs/design/hub-headers.md §8.1); implementors narrow
 * getObject() non-virtually where they bind M (PlayerGroupMember: Player). Held by `Ref<TeamMember>` in GeneralTeam, so it declares the
 * reference count operations (§9.2). Written with the S0b objects group because TemporaryPlayerTeam (a hub) derives GeneralTeam.
 *
 * @author ATracer
 */
class TeamMember {
public:
	virtual int32_t getObjectId() = 0;

	virtual std::string getName() = 0;

	/** Java return type M */
	virtual runtime::Ptr<gameobjects::AionObject> getObject() = 0;

	/** C++ only: Ref<TeamMember> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;

	virtual ~TeamMember() = default;

protected:
	TeamMember() = default;
	TeamMember(const TeamMember&) = default;
	TeamMember& operator=(const TeamMember&) = default;
};

} // namespace aion::gameserver::model::team
