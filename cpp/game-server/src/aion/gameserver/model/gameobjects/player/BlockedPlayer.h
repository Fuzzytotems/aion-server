#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Represents a player who has been blocked. Blocks via a player's CommonData
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `BlockList.blockedList`), created with create().
 *
 * @author Ben
 */
class BlockedPlayer : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t objId;
	const std::string name;
	runtime::Field<std::string> reason;

protected:
	BlockedPlayer(int32_t objId, std::string_view name, std::string_view reason);
	~BlockedPlayer() override;

public:
	/** Java: new BlockedPlayer(objId, name, reason) */
	static runtime::Ref<BlockedPlayer> create(int32_t objId, std::string_view name, std::string_view reason);

	int32_t getObjId() const { return objId; }

	std::string getName() const { return name; }

	/** synchronized */
	std::string getReason();

	/** synchronized */
	void setReason(std::string_view reason);
};

} // namespace aion::gameserver::model::gameobjects::player
