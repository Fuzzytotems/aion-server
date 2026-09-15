#include "aion/gameserver/skillengine/effect/BufEffect.h"

#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void BufEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void BufEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

std::vector<runtime::Ref<gameserver::model::stats::calc::functions::IStatFunction>> BufEffect::getModifiers(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
