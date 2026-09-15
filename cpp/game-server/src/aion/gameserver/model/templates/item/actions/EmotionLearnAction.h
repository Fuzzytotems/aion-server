#pragma once

#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.EmotionLearnAction. @author Mr. Poke */
class EmotionLearnAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.xml.inc"
public:
	/**
	 * Learnable IDs as of 4.8:<br>
	 * 64 - 155<br>
	 * <br>
	 * Not learnable known valid IDs:<br>
	 * 1 - 35 - default emotions<br>
	 * >10000 - housing emotions (10006/10007 lay in left/right side of a bed, 10008 sitting on a chair, ...)
	 *
	 * @return True if there exists a learn template for given emotion. False means it's either a default or an invalid emotion.
	 */
	static bool isLearnable(int32_t emotionId);
};

} // namespace aion::gameserver::model::templates::item::actions
