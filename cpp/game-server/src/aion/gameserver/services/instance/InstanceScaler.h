#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/services/instance/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::services::instance {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * Scaling and InstanceScalerStatFunction are package-private nested classes used in signatures.
 *
 * @author Minzi90
 */
class InstanceScaler : public runtime::Immortal, public model::stats::calc::StatOwner {
public:
	class Scaling;
	class InstanceScalerStatFunction;
	class Scaling : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	protected:
		Scaling() = default;

	public:
		/** C++ only (hub-headers.md §10.1): Java `new Scaling()` */
		static runtime::Ref<Scaling> create();
		runtime::Field<int32_t> playerCount{};
		// Java: = Collections.emptyList(); null until the first update (the port creates the empty list)
		runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<InstanceScaler::InstanceScalerStatFunction>>>> statFunctions{};
		bool update(world::WorldMapInstance& instance);
		bool isLowLevelInstanceWithHighLevelPlayers(world::WorldMapInstance& instance, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players);
		int32_t getInstanceEnterMinLevel(world::WorldMapInstance& instance, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players);
		std::vector<runtime::Ref<InstanceScaler::InstanceScalerStatFunction>> createStatFunctions(world::WorldMapInstance& instance, int32_t playerCount);
	protected:
		~Scaling() override;
	};
	class InstanceScalerStatFunction : public runtime::RefCounted, public model::stats::calc::functions::StatFunction {
		AION_MAKE_REF_FRIEND
	public:
		const float rate;
	protected:
		InstanceScalerStatFunction(model::stats::container::StatEnum stat, float rate);
	public:
		static runtime::Ref<InstanceScaler::InstanceScalerStatFunction> create(model::stats::container::StatEnum statValue, float rateValue);
		/** C++ only: a run-time StatFunction is RefCounted (StatFunction.h, hub-headers.md §9.2) */
		void retain() const noexcept override { runtime::RefCounted::retain(); }
		void release() const noexcept override { runtime::RefCounted::release(); }
		void apply(model::stats::calc::Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;
		int32_t getPriority() const override;
	protected:
		~InstanceScalerStatFunction() override;
	};
private:
	// Collections.synchronizedMap: the port locks the map; destroyInstance must remove the instance (strong keys)
	// defined in InstanceScaler.cpp (the map's destructor needs the complete WorldMapInstance)
	// fieldmap.toml: java.util.WeakHashMap has no shim; weak references are Ref (design §5.1)
	static runtime::HashMap<runtime::Ref<world::WorldMapInstance>, runtime::Ref<InstanceScaler::Scaling>> scalings;
	InstanceScaler();
	~InstanceScaler();
	/** Java: private static final InstanceScaler INSTANCE (the StatOwner of the scaling stat functions) */
	static InstanceScaler& getInstance();

public:
	/** C++ only: StatOwner is held by Ref (hub-headers.md §9.2); the singleton is Immortal, so these are no-ops. */
	void retain() const noexcept override {}
	void release() const noexcept override {}
	static void onEnterInstance(model::gameobjects::player::Player& player);
	static void onBeforeSpawn(model::gameobjects::Npc& npc);
private:
	static void rescale(world::WorldMapInstance& instance, InstanceScaler::Scaling& scaling);
public:
	static bool canScale(world::WorldMapInstance& instance);
private:
	static bool shouldScale(model::gameobjects::Npc& npc, world::WorldMapInstance& instance);
	static void scaleNpc(model::gameobjects::Npc& npc, InstanceScaler::Scaling& scaling);
public:
	static float calculateMultiplier(world::WorldMapInstance& instance, float floor, float scaleFactor, int32_t playerCount);
};

} // namespace aion::gameserver::services::instance
