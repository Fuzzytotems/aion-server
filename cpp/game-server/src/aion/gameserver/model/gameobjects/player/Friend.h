#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/FriendList_Status.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `FriendList.friends`), created with create().
 *
 * @author Ben
 */
class Friend : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<PlayerCommonData>> pcd{};
	runtime::Field<std::string> memo;

protected:
	Friend(PlayerCommonData& pcd, std::string_view memo);
	~Friend() override;

public:
	/** Java: new Friend(pcd, memo) */
	static runtime::Ref<Friend> create(PlayerCommonData& pcd, std::string_view memo);

	/** Returns the status of this player */
	FriendList_Status getStatus();

	void setPCD(runtime::Ptr<PlayerCommonData> pcd);

	/** @return name of this friend */
	std::string getName();

	int32_t getLevel();

	std::string getNote();

	PlayerClass getPlayerClass();

	Gender getGender();

	int32_t getMapId();

	/** Gets the last time this player was online in seconds since epoch */
	int32_t getLastOnlineEpochSeconds();

	int32_t getObjectId();

	/** synchronized */
	std::string getFriendMemo();

	/** synchronized */
	void setFriendMemo(std::string_view memo);
};

} // namespace aion::gameserver::model::gameobjects::player
