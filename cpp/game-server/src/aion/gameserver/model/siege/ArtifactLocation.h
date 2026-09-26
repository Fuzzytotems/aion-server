#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"

namespace aion::gameserver::model::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Source
 */
class ArtifactLocation : public SiegeLocation {
	AION_MAKE_REF_FRIEND
private:
	// fieldmap.toml: null until the first setStatus (getStatus maps null to IDLE, ArtifactLocation.java:71)
	runtime::Field<std::optional<ArtifactStatus>> status{};
	runtime::Field<int64_t> lastArtifactActivation{};

protected:
	explicit ArtifactLocation(const templates::siegelocation::SiegeLocationTemplate* template_);

public:
	static runtime::Ref<ArtifactLocation> create(const templates::siegelocation::SiegeLocationTemplate* value);

	int32_t getNextState() override;

	int64_t getLastActivation() const { return this->lastArtifactActivation.get(); }

	void setInitialDelay(int64_t capturedTime);

	void setLastActivation(int64_t lastActivation) { this->lastArtifactActivation.set(lastActivation); }

	int32_t getCoolDown();

	std::string getL10n();

	bool isStandAlone();

	runtime::Ptr<FortressLocation> getOwningFortress();

	ArtifactStatus getStatus();

	void setStatus(std::optional<ArtifactStatus> value) { this->status.set(value); }

protected:
	~ArtifactLocation() override;
};

} // namespace aion::gameserver::model::siege
