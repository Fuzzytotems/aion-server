#include "aion/gameserver/model/base/PanesterraArtifact.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/base/PanesterraBaseLocation.h"

namespace aion::gameserver::model::base {

// Java PanesterraArtifact.java:8-10
PanesterraArtifact::PanesterraArtifact(PanesterraBaseLocation& loc) : PanesterraBase(loc) {
}

runtime::Ref<PanesterraArtifact> PanesterraArtifact::create(PanesterraBaseLocation& loc) {
	return runtime::makeRef<PanesterraArtifact>(loc);
}

// Java PanesterraArtifact.java:12-15
int32_t PanesterraArtifact::getBossSpawnDelay() {
	return 5 * 60000; // Retail delay
}

PanesterraArtifact::~PanesterraArtifact() = default;

} // namespace aion::gameserver::model::base
