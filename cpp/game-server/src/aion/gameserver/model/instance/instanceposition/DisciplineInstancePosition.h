#pragma once

#include "aion/gameserver/model/instance/instanceposition/GeneralInstancePosition.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::instanceposition {

/**
 * The discipline arenas' four start spots in each of the four zones.
 *
 * @author xTz
 */
class DisciplineInstancePosition : public GeneralInstancePosition {
	AION_MAKE_REF_FRIEND
protected:
	DisciplineInstancePosition();
	~DisciplineInstancePosition() override;

public:
	static runtime::Ref<DisciplineInstancePosition> create();

	void port(gameobjects::player::Player& player, int32_t zone, int32_t position) override;
};

} // namespace aion::gameserver::model::instance::instanceposition
