#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/transfers/fwd.h"

namespace aion::gameserver::services::transfers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author KID
 */
class TransferablePlayer : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	runtime::Field<int32_t> playerId{};
	runtime::Field<int32_t> accountId{};
	runtime::Field<int32_t> targetAccountId{};
	runtime::Field<runtime::Ref<model::gameobjects::player::Player>> player{};
	runtime::Field<int8_t> targetServerId{};
	runtime::Field<int32_t> taskId{};

protected:
	TransferablePlayer(int32_t playerId, int32_t accountId, int32_t targetAccountId);

public:
	static runtime::Ref<TransferablePlayer> create(int32_t value, int32_t accountIdValue, int32_t targetAccountIdValue);

protected:
	~TransferablePlayer() override;
};

} // namespace aion::gameserver::services::transfers
