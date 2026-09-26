#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/findGroup/FindGroupEntry.h"
#include "aion/gameserver/model/gameobjects/findGroup/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::gameobjects::findGroup {

/**
 * Find Group
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author MrPoke
 */
class GroupRecruitment final : public runtime::RefCounted, public FindGroupEntry {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<AionObject> object;
	runtime::Field<std::string> message{};
	runtime::Field<int32_t> groupType{};
	runtime::Field<int32_t> classId{-1};
	runtime::Field<int32_t> level{-1};
	runtime::Field<int32_t> lastUpdate{}; // Java: = (int) (System.currentTimeMillis() / 1000)

protected:
	GroupRecruitment(AionObject& object, std::string_view message, int32_t groupType);

public:
	static runtime::Ref<GroupRecruitment> create(AionObject& value, std::string_view messageValue, int32_t groupTypeValue);

	runtime::Ptr<AionObject> getObject() const { return this->object; }

	int32_t getObjectId();

	std::string getMessage() const { return this->message.get(); }

	void setMessage(std::string_view value) { this->message.set(std::string(value)); }

	int32_t getGroupType() const { return this->groupType.get(); }

	void setGroupType(int32_t value) { this->groupType.set(value); }

	int32_t getClassId();

	void setClassId(int32_t value) { this->classId.set(value); }

	int32_t getMinLevel();

	void setLevel(int32_t value) { this->level.set(value); }

	int32_t getMaxLevel();

	std::string getName();

	int32_t getSize();

	int32_t getLastUpdate() const { return this->lastUpdate.get(); }

	void updateLastUpdate();

	Race getRace();

	/** C++ only: FindGroupEntry retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~GroupRecruitment() override;
};

} // namespace aion::gameserver::model::gameobjects::findGroup
