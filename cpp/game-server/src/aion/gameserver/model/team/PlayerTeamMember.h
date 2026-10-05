#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team {

/**
 * A player in a group or an alliance.
 * <p>
 * C++: K4 (a member of GeneralTeam.members), RefCounted and created with create(); it is the first implementor of TeamMember with a runtime
 * base, so it forwards retain()/release() (hub-headers.md §9.2, runtime-architecture.md §14.2(d)). getObject() narrows the erased
 * `AionObject` result to Player non-virtually (hub-headers.md §8.2); getObjectPtr() is the erased TeamMember override.
 *
 * @author ATracer
 */
class PlayerTeamMember : public runtime::RefCounted, public TeamMember {
	AION_MAKE_REF_FRIEND
public:
	/** Java package-private */
	const runtime::Ref<gameobjects::player::Player> player;

private:
	runtime::Field<int64_t> lastOnlineTime;

protected:
	explicit PlayerTeamMember(gameobjects::player::Player& player);
	~PlayerTeamMember() override;

public:
	static runtime::Ref<PlayerTeamMember> create(gameobjects::player::Player& player);

	int32_t getObjectId() override;

	std::string getName() override;

	/** Java return type Player (the TeamMember<Player> binding) */
	runtime::Ptr<gameobjects::AionObject> getObject() override;

	/** Narrowing accessor: Java getObject() of TeamMember<Player> */
	gameobjects::player::Player& getPlayer() const { return *player; }

	int64_t getLastOnlineTime() const { return lastOnlineTime.get(); }

	void updateLastOnlineTime();

	bool isOnline();

	float getX();

	float getY();

	float getZ();

	int8_t getHeading();

	int8_t getLevel();

	void retain() const noexcept override { RefCounted::retain(); }
	void release() const noexcept override { RefCounted::release(); }
};

} // namespace aion::gameserver::model::team
