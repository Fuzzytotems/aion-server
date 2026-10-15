#include "aion/gameserver/skillengine/effect/TransformEffect.h"

#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/TransformType.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using runtime::Ptr;

void TransformEffect::applyEffect(model::Effect& effect) const {
	/**
	 * TODO need more info fix for cases like use itemId: 160010206(Dignified Wyvern Form Candy) after that use cannon skill(ex. 20365) -> candy
	 * should be removed
	 */
	if (type == model::TransformType::FORM1 && panelid > 0) {
		if (effect.getEffected()->getTransformModel().isActive()) {
			effect.getEffected()->getEffectController()->removeTransformEffects();
		}
	}

	effect.addToEffectedController();
}

void TransformEffect::endEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();

	// Java: the break leaves only the inner loop, so the outer one goes on and the last abnormal effect holding another transformation wins
	const TransformEffect* temp = nullptr;
	for (const Ptr<model::Effect>& tmp : effected->getEffectController()->getAbnormalEffects()) {
		for (const EffectTemplate* effectTemplate : tmp->getEffectTemplates()) {
			const auto* transformEffect = dynamic_cast<const TransformEffect*>(effectTemplate);
			if (transformEffect != nullptr && transformEffect->getTransformId() != model) {
				temp = transformEffect;
				break;
			}
		}
	}
	if (temp != nullptr)
		effected->getTransformModel().apply(temp->getTransformId(), temp->getTransformType(), temp->getPanelId(), temp->cantUseSkills(),
			temp->cantMove(), temp->cantRecall(), temp->cantJump(), temp->cantAttack(), temp->cantUseItems(), temp->cantFly());
	else
		effected->endTransformation();
}

void TransformEffect::startEffect(model::Effect& effect) const {
	effect.getEffected()->getTransformModel().apply(getTransformId(), getTransformType(), getPanelId(), cantUseSkills(), cantMove(), cantRecall(),
		cantJump(), cantAttack(), cantUseItems(), cantFly());
}

} // namespace aion::gameserver::skillengine::effect
