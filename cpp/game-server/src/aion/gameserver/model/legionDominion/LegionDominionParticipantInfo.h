#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/legionDominion/fwd.h"

namespace aion::gameserver::model::legionDominion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Yeats
 */
class LegionDominionParticipantInfo : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> legionId{};
	runtime::Field<int32_t> points{};
	runtime::Field<int32_t> time{};
	// fieldmap.toml: nullable Timestamp (hub-headers.md §6): null until the legion took part
	runtime::Field<std::optional<commons::database::Timestamp>> date{};

protected:
	/** Java: the implicit default constructor */
	LegionDominionParticipantInfo();

public:
	static runtime::Ref<LegionDominionParticipantInfo> create();

	int32_t getLegionId() const { return this->legionId.get(); }

	int32_t getPoints() const { return this->points.get(); }

	int32_t getTime() const { return this->time.get(); }

	int64_t getDate();

	std::optional<commons::database::Timestamp> getDateAsTimeStamp() const { return this->date.get(); }

	void setLegionId(int32_t value) { this->legionId.set(value); }

	void setPoints(int32_t value) { this->points.set(value); }

	void setTime(int32_t value) { this->time.set(value); }

	void setDate(std::optional<commons::database::Timestamp> timestamp) { date.set(timestamp); }

	std::string getLegionName();

protected:
	~LegionDominionParticipantInfo() override;
};

} // namespace aion::gameserver::model::legionDominion
