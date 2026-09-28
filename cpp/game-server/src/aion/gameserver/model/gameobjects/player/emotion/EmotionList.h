#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/emotion/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player::emotion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`PartSlot<EmotionList>`, cycles review, set with setEmotions),
 * bound to the player in the constructor. The emotion map is created by the first add (`Field<Ref<RcLinkedHashMap>>`, insertion order is the
 * packet order). getEmotions() returns a snapshot (Java: the map's values or an empty list).
 *
 * @author MrPoke
 */
class EmotionList : public runtime::OwnedPart {
private:
	runtime::Field<runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Emotion>>>> emotions{};
	runtime::OwnerRef<Player> owner;

public:
	explicit EmotionList(Player& owner);

	~EmotionList() override;

	void add(int32_t emotionId, int32_t dispearTime, bool isNew);

	void remove(int32_t emotionId);

	bool contains(int32_t emotionId);

	bool canUse(int32_t emotionId);

	std::vector<runtime::Ptr<Emotion>> getEmotions();
};

} // namespace aion::gameserver::model::gameobjects::player::emotion
