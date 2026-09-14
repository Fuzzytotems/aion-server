#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/legionDominion/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"

namespace aion::gameserver::model::legionDominion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Yeats, Sykra
 */
class LegionDominionLocation : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const templates::LegionDominionLocationTemplate* template_;
	const std::string zoneName;
	runtime::Field<int32_t> legionId{};
	// fieldmap: nullable Timestamp (hub-headers.md §6): null while no legion occupies the territory
	runtime::Field<std::optional<commons::database::Timestamp>> occupiedDate{};
	runtime::Field<runtime::Ref<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>>> participantInfo{};

protected:
	explicit LegionDominionLocation(const templates::LegionDominionLocationTemplate* template_);

public:
	static runtime::Ref<LegionDominionLocation> create(const templates::LegionDominionLocationTemplate* value);

	int32_t getLocationId();

	int32_t getWorldId();

	Race getRace();

	std::string getL10n();

	std::string getZoneNameAsString() const { return this->zoneName; }

	int32_t getLegionId() const { return this->legionId.get(); }

	const templates::LegionDominionInvasionRift* getInvasionRift();

	void setLegionId(int32_t value) { this->legionId.set(value); }

	std::optional<commons::database::Timestamp> getOccupiedDate() const { return this->occupiedDate.get(); }

	void setOccupiedDate(std::optional<commons::database::Timestamp> timestamp) { occupiedDate.set(timestamp); }

	/** Java: the live map (a TreeMap ordered by legion id) */
	runtime::Ptr<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> getParticipantInfo() const { return participantInfo.get(); }

	/** @param info the map to keep (Java stores the caller's map) */
	void setParticipantInfo(runtime::Ptr<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> info);

	std::vector<runtime::Ptr<LegionDominionParticipantInfo>> getLegionRanking(bool removeNonEligibleLegions);

	std::unordered_map<int32_t, std::vector<const templates::LegionDominionReward*>> getRewards();

	bool join(int32_t legionId);

private:
	void store(LegionDominionParticipantInfo& info, bool isNew);

public:
	runtime::Ptr<LegionDominionParticipantInfo> getParticipantInfo(int32_t legionId);

	void updateRanking();

	void reset(); // synchronized

protected:
	~LegionDominionLocation() override;
};

} // namespace aion::gameserver::model::legionDominion
