#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

void AbstractOverTimeEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void AbstractOverTimeEffect::startEffect(model::Effect& effect) const {
	startEffect(effect, std::nullopt);
}

// Stored lambda com.aionemu.gameserver.skillengine.effect.AbstractOverTimeEffect@L55:72 (periodic task in Effect.periodicTasks, pin {this,
// &effect}; the template is immortal static data, so its pin retains nothing): cycles.toml "AbstractOverTimeEffect@L55:72#effect" - java-hook:
// Effect.stopTasks (from Effect.endEffect) cancels the periodic task held in Effect.periodicTasks, which releases the pin and the callable
void AbstractOverTimeEffect::startEffect(model::Effect& effect, std::optional<AbnormalState> abnormal) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	if (abnormal) {
		effect.setAbnormal(*abnormal);
		effected->getEffectController()->setAbnormal(*abnormal);
	}
	// TODO figure out what to do with such cases
	if (checktime == 0)
		return;
	// Some skills have an effective duration of 2000 (see getDuration2) and a checktime of 1000 (e.g. Ripple of Purification).
	// On retail, these skills are applied once instead of twice, so we slightly increase the initialDelay to prevent this from happening.
	// Java: `long initialDelay = 300 + checktime` - an int sum (wrapping), widened to long afterwards
	int64_t initialDelay = static_cast<int32_t>(300u + static_cast<uint32_t>(checktime));
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin{this, &effect}, [this, &effect] {
		onPeriodicAction(effect);
	}, initialDelay, checktime);
	effect.setPeriodicTask(task, position);
}

void AbstractOverTimeEffect::endEffect(model::Effect& effect, std::optional<AbnormalState> abnormal) const {
	if (abnormal)
		effect.getEffected()->getEffectController()->unsetAbnormal(*abnormal);
}

} // namespace aion::gameserver::skillengine::effect
