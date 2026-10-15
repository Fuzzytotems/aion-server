#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The aggro list part of players and summons (`createAggroList()` returns
 * `std::make_unique<PlayerAggroList>(*this)`), an OwnedPart bound in the AggroList constructor.
 *
 * @author ATracer
 */
class PlayerAggroList : public AggroList {
public:
	explicit PlayerAggroList(model::gameobjects::Creature& owner);
	~PlayerAggroList() override;

protected:
	bool isAware(runtime::Ptr<model::gameobjects::Creature> creature) override;
};

} // namespace aion::gameserver::controllers::attack
