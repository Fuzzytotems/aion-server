#pragma once

#include "aion/gameserver/model/instance/instanceposition/GeneralInstancePosition.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instanceposition {

/**
 * The arena of glory's start spots per zone.
 *
 * @author xTz
 */
class GloryInstancePosition : public GeneralInstancePosition {
	AION_MAKE_REF_FRIEND
protected:
	GloryInstancePosition();
	~GloryInstancePosition() override;

public:
	static runtime::Ref<GloryInstancePosition> create();

	void port(gameobjects::player::Player& player, int32_t zone, int32_t position) override;
};

} // namespace aion::gameserver::model::instance::instanceposition
