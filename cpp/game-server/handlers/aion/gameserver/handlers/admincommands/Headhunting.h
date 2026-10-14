#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //headhunting: the seasonal headhunting event's analysis, results, rewards and finalization (D12, owner 2026-10-13).
 * <p>
 * C++: Java's EnumMap&lt;Race, EnumMap&lt;PlayerClass, List&lt;Headhunter&gt;&gt;&gt; results is a std::map keyed by the enums (EnumMap iterates in
 * ordinal order) is the HashMap shim of fieldmap.json, iterated in the enums' ordinal order as the EnumMap does; null is a null Ref. The sort of
 * analyze uses Headhunter.compareTo, which lane B ports (M5i I-4): with two or more hunters of one class and race it is AION_UNPORTED until
 * that merges.
 *
 * @author Estrayl (Java)
 */
class Headhunting : public AdminCommand {
public:
	Headhunting();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	using HunterList = runtime::RcArrayList<runtime::Ref<model::event::Headhunter>>;
	using ClassMap = runtime::RcHashMap<model::PlayerClass, runtime::Ref<HunterList>>;
	using Results = runtime::RcHashMap<model::Race, runtime::Ref<ClassMap>>;

	/** Java: private final Map<Integer, List<RewardItem>> rewards = new LinkedHashMap<>() (rank -> items) */
	runtime::LinkedHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<RewardItem>>>> rewards{AION_LOCK_CLASS(Headhunting::rewards)};
	/** Java: private final List<RewardItem> consolationRewards = new ArrayList<>() */
	runtime::ArrayList<runtime::Ref<RewardItem>> consolationRewards{AION_LOCK_CLASS(Headhunting::consolationRewards)};
	/** Java: private Map<Race, Map<PlayerClass, List<Headhunter>>> results (null: not analyzed) */
	runtime::Field<runtime::Ref<runtime::RcHashMap<model::Race, runtime::Ref<runtime::RcHashMap<model::PlayerClass, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::event::Headhunter>>>>>>>> results{};

	/** Java: List.of(items...) */
	static runtime::Ref<runtime::RcArrayList<runtime::Ref<RewardItem>>> listOf(std::initializer_list<runtime::Ref<RewardItem>> items);

	/** Java: the keys of an EnumMap, in ordinal order */
	template <typename Map>
	static auto keysInOrdinalOrder(const Map& map);

	void addKills(Player& admin, int32_t playerId, int32_t kills);
	bool analyzeSeason(Player& admin);
	void showResults(Player& admin);
	void showRewards(Player& admin);
	void rewardPlayers(Player& admin);
	void finalizeSeason(Player& admin, bool shouldReward);
};

} // namespace aion::gameserver::handlers::admincommands
