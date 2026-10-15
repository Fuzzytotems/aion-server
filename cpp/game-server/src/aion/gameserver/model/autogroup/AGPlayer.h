#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::autogroup {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class AGPlayer : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t objectId_;
	const Race race_;
	const PlayerClass playerClass_;
	const std::string name_;

protected:
	/** Java: the canonical record constructor */
	AGPlayer(int32_t objectId, Race race, PlayerClass playerClass, std::string_view name);

	explicit AGPlayer(gameobjects::player::Player& player);

public:
	static runtime::Ref<AGPlayer> create(int32_t objectId, Race race, PlayerClass playerClass, std::string_view name);

	static runtime::Ref<AGPlayer> create(gameobjects::player::Player& player);

	int32_t objectId() const { return this->objectId_; }

	Race race() const { return this->race_; }

	PlayerClass playerClass() const { return this->playerClass_; }

	std::string name() const { return this->name_; }

protected:
	~AGPlayer() override;
};

} // namespace aion::gameserver::model::autogroup
