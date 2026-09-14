#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/findGroup/FindGroupEntry.h"
#include "aion/gameserver/model/gameobjects/findGroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::findGroup {

class ServerWideGroup final : public runtime::RefCounted, public FindGroupEntry {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<player::Player>> members{AION_LOCK_CLASS(ServerWideGroup::members)}; // Java: = new ArrayList<>()
	const int32_t instanceMaskId;
	const int32_t minMembers;
	runtime::Field<std::string> message{};
	runtime::Field<int32_t> lastUpdate{};

protected:
	ServerWideGroup(player::Player& recruiter, int32_t instanceMaskId, int32_t minMembers, std::string_view message);

public:
	static runtime::Ref<ServerWideGroup> create(player::Player& recruiter, int32_t value, int32_t minMembersValue, std::string_view messageValue);

	std::vector<runtime::Ptr<player::Player>> getMembers();

	int32_t getInstanceMaskId() const { return this->instanceMaskId; }

	int32_t getMinMembers() const { return this->minMembers; }

	std::string getMessage() const { return this->message.get(); }

	void setMessage(std::string_view value) { this->message.set(std::string(value)); }

	int32_t getLastUpdate() const { return this->lastUpdate.get(); }

	void setLastUpdate();

	runtime::Ptr<player::Player> getRecruiter();

	int32_t getId();

	Race getRace();

	int32_t getMinLevel();

	int32_t getMaxLevel();

	/** C++ only: FindGroupEntry retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~ServerWideGroup() override;
};

} // namespace aion::gameserver::model::gameobjects::findGroup
