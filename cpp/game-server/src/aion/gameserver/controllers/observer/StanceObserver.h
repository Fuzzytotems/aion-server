#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Watches all conditions when a stance needs to be removed
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted ActionObserver (fieldmap K4, `PlayerController::stanceObserver`),
 * created with create().
 *
 * @author Neon
 */
class StanceObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const int32_t stanceSkillId;

protected:
	StanceObserver(model::gameobjects::player::Player& player, int32_t stanceSkillId);
	~StanceObserver() override;

public:
	/** Java: new StanceObserver(player, stanceSkillId) */
	static runtime::Ref<StanceObserver> create(model::gameobjects::player::Player& player, int32_t stanceSkillId);

	int32_t getStanceSkillId() const { return stanceSkillId; }

	void startSkillCast(skillengine::model::Skill& skill) override;

	void itemused(model::gameobjects::Item& item) override;

	void abnormalsetted(skillengine::effect::AbnormalState state) override;
};

} // namespace aion::gameserver::controllers::observer
