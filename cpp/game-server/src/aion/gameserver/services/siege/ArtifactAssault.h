#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/siege/Assault.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

/**
 * TODO: onAssaultFail(), if BalaurAssaulter fails to capture
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Luzien, Whoop
 */
class ArtifactAssault : public Assault {
	AION_MAKE_REF_FRIEND
protected:
	explicit ArtifactAssault(ArtifactSiege& siege);

public:
	static runtime::Ref<ArtifactAssault> create(ArtifactSiege& siege);

	void handleAssault() override;

	void onAssaultFinish(bool captured) override;

private:
	void spawnAssaulter();

	int32_t getAssaulterIdByBossLvl();

protected:
	~ArtifactAssault() override;
};

} // namespace aion::gameserver::services::siege
