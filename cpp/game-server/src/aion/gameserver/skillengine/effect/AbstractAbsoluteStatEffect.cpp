#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.h"

#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

std::vector<runtime::Ref<gameserver::model::stats::calc::functions::IStatFunction>> AbstractAbsoluteStatEffect::getModifiers(
	model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
