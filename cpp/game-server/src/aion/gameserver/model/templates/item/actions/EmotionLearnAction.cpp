#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/sync/LockClass.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

/**
 * Java `private static final Set<Integer> LEARNABLE_IDS = ConcurrentHashMap.newKeySet()`. C++ (ported by P4-09 for the M4 load path, P5-07 has no
 * lane in wave 3b-1): the Monitor-guarded HashSet shim instead of the ConcurrentKeySet shim, because the item templates are also bound outside
 * a TaskScope (tests) and a set of ints needs no read barrier; add and contains are thread-safe either way.
 */
runtime::HashSet<int32_t> learnableIds{AION_LOCK_CLASS(EmotionLearnAction::LEARNABLE_IDS)};

} // namespace

void EmotionLearnAction::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	learnableIds.add(emotionId);
}

bool EmotionLearnAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void EmotionLearnAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

bool EmotionLearnAction::isLearnable(int32_t emotionId) {
	return learnableIds.contains(emotionId);
}

} // namespace aion::gameserver::model::templates::item::actions
