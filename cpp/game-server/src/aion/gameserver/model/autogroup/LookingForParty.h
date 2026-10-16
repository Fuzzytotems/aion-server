#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::autogroup {

/**
 * Java implements Comparable<LookingForParty>.
 *
 * @author xTz
 */
class LookingForParty : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::HashMap<int32_t, runtime::Ref<AGPlayer>> members{AION_LOCK_CLASS(LookingForParty::members)};
	const EntryRequestType ert;
	const Race race;
	const int64_t registrationTime; // Java: = System.currentTimeMillis()
	const int32_t maskId;
	runtime::Field<int64_t> startEnterTime{};
	runtime::Field<int32_t> leaderObjId{};

protected:
	LookingForParty(gameobjects::player::Player& player, EntryRequestType ert, int32_t maskId);

public:
	static runtime::Ref<LookingForParty> create(gameobjects::player::Player& player, EntryRequestType value, int32_t maskIdValue);

private:
	/** @return the newly created members (Java `new AGPlayer(player)`), hub-headers.md §5 */
	std::unordered_map<int32_t, runtime::Ref<AGPlayer>> createMembers(gameobjects::player::Player& player);

public:
	runtime::HashMap<int32_t, runtime::Ref<AGPlayer>>& getMembers() { return this->members; }

	bool isMember(int32_t objectId);

	void unregisterMember(std::optional<int32_t> objectId);

	EntryRequestType getEntryRequestType() const { return this->ert; }

	Race getRace() const { return this->race; }

	int64_t getRegistrationTime() const { return this->registrationTime; }

	int32_t getMaskId() const { return this->maskId; }

	int32_t getLeaderObjId() const { return this->leaderObjId.get(); }

	void setLeaderObjId(int32_t value) { this->leaderObjId.set(value); }

	bool isLeader(int32_t objectId);

	void setStartEnterTime();

	bool isOnStartEnterTask();

	int32_t compareTo(const LookingForParty& lfp) const;

protected:
	~LookingForParty() override;
};

} // namespace aion::gameserver::model::autogroup
