#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "aion/gameserver/model/autogroup/AutoInstance.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"

namespace aion::gameserver::model::autogroup {

/**
 * The Arena of Harmony and the Harmonious Training Center: two parties of up to three of one race each.
 * <p>
 * Java: model/autogroup/AutoHarmonyInstance.java. `groups` is the fieldmap's HashMap<int32_t, Ref<RcArrayList<Ref<AGPlayer>>>> under its own
 * Monitor; the `synchronized addLookingForParty` locks the object's Monitor (SYNCHRONIZED(*this)). The casts of the instance's score to
 * HarmonyArenaScore are runtime::cast (Java's ClassCastException).
 *
 * @author xTz, Estrayl
 */
class AutoHarmonyInstance : public AutoInstance {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<AutoHarmonyInstance> create(AutoGroupType agt);

	void onInstanceCreate(world::WorldMapInstance& instance) override;

	AGQuestion addLookingForParty(LookingForParty& lookingForParty) override;

	void onPressEnter(gameobjects::player::Player& player) override;

	void onEnterInstance(gameobjects::player::Player& player) override;

	void onLeaveInstance(gameobjects::player::Player& player) override;

	void unregister(gameobjects::player::Player& player) override;

protected:
	explicit AutoHarmonyInstance(AutoGroupType agt);
	~AutoHarmonyInstance() override;

private:
	using Group = runtime::RcArrayList<runtime::Ref<AGPlayer>>;

	std::vector<runtime::Ptr<gameobjects::player::Player>> findPlayersInInstance(Group& group);

	/** Java: the Map.Entry of the player's group (its key 0 or 1 and the list), null if he is in neither */
	std::optional<std::pair<int32_t, runtime::Ref<Group>>> getGroupEntry(int32_t playerObjId);

	AGQuestion canAddParty(Group& group, LookingForParty& lfp);

	/** Java: private final HashMap<Integer, List<AGPlayer>> groups = new HashMap<>() */
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<AGPlayer>>>> groups{AION_LOCK_CLASS(AutoHarmonyInstance::groups)};
};

} // namespace aion::gameserver::model::autogroup
