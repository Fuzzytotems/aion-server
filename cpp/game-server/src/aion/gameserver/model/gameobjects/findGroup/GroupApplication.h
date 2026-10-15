#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/findGroup/FindGroupEntry.h"
#include "aion/gameserver/model/gameobjects/findGroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::findGroup {

class GroupApplication final : public runtime::RefCounted, public FindGroupEntry {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<gameobjects::player::Player> player;
	runtime::Field<std::string> message{};
	runtime::Field<int32_t> groupType{};
	runtime::Field<int32_t> classId{};
	runtime::Field<int32_t> level{};
	runtime::Field<int32_t> lastUpdate{}; // Java: = (int) (System.currentTimeMillis() / 1000)

protected:
	GroupApplication(gameobjects::player::Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level);

public:
	static runtime::Ref<GroupApplication> create(gameobjects::player::Player& value, std::string_view messageValue, int32_t groupTypeValue,
		int32_t classIdValue, int32_t levelValue);

	runtime::Ptr<gameobjects::player::Player> getPlayer() const { return this->player; }

	std::string getMessage() const { return this->message.get(); }

	void setMessage(std::string_view value) { this->message.set(std::string(value)); }

	int32_t getGroupType() const { return this->groupType.get(); }

	void setGroupType(int32_t value) { this->groupType.set(value); }

	int32_t getClassId() const { return this->classId.get(); }

	void setClassId(int32_t value) { this->classId.set(value); }

	int32_t getLevel() const { return this->level.get(); }

	void setLevel(int32_t value) { this->level.set(value); }

	int32_t getLastUpdate() const { return this->lastUpdate.get(); }

	void updateLastUpdate();

	/** C++ only: FindGroupEntry retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~GroupApplication() override;
};

} // namespace aion::gameserver::model::gameobjects::findGroup
