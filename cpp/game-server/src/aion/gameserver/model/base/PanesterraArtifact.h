#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/PanesterraBase.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.PanesterraArtifact (a PanesterraBase): the artifact base of Panesterra (no assault).
 *
 * @author Estrayl
 */
class PanesterraArtifact : public PanesterraBase {
	AION_MAKE_REF_FRIEND

protected:
	explicit PanesterraArtifact(PanesterraBaseLocation& loc);

public:
	static runtime::Ref<PanesterraArtifact> create(PanesterraBaseLocation& loc);

protected:
	int32_t getBossSpawnDelay() override;

	~PanesterraArtifact() override;
};

} // namespace aion::gameserver::model::base
