#pragma once

#include <cstdint>
#include <functional>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/event/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ATracer, Source, xTz, Neon
 */
class RespawnService {
private:
	/** Java: static class DecayTask implements Runnable (used only by the bodies, defined in RespawnService.cpp) */
	class DecayTask;
public:
	// Java implements Runnable
	class RespawnTask : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const runtime::Ref<model::templates::spawns::SpawnTemplate> spawnTemplate{};
		const int32_t instanceId{};
		const int32_t oldObjectId{};
		runtime::Field<runtime::FutureRef> future{};
		runtime::Field<bool> releaseIdOnUnregister{};
	protected:
		explicit RespawnTask(model::gameobjects::VisibleObject& object);
	public:
		static runtime::Ref<RespawnService::RespawnTask> create(model::gameobjects::VisibleObject& object);
		void run(); // @Override of a Java library type
		bool tryRegisterOnEventEndTask();
		void respawn();
		bool setReleaseIdOnCompletion();
		void onUnregister();
		void cancel();
		void unregister();
	protected:
		~RespawnTask() override;
	};
	static constexpr int32_t IMMEDIATE_DECAY = 2 * 1000;
	static constexpr int32_t WITH_DROP_DECAY = 5 * 60 * 1000;
private:
	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<RespawnService::RespawnTask>> pendingRespawns{
		AION_LOCK_CLASS(RespawnService::pendingRespawns#stripe)};
public:
	/**
	 * Schedules decay (despawn) of the npc with the default delay time. If there is already a decay task, it will be replaced with this one.
	 */
	static runtime::FutureRef scheduleDecayTask(model::gameobjects::Npc& npc);
	/**
	 * Schedules decay (despawn) of the object with the specified delay. If the object is a creature type and there is already a decay task registered,
	 * it will be replaced with this one.
	 */
	static runtime::FutureRef scheduleDecayTask(model::gameobjects::VisibleObject& visibleObject, int64_t delay);
	/**
	 * Schedules respawn of the object. Objects without respawn time (like in instances) or without spawn templates will not respawn. If the object is a
	 * creature type and there is already a respawn task registered, it will be replaced with this one.
	 */
	static runtime::Ptr<RespawnService::RespawnTask> scheduleRespawn(model::gameobjects::VisibleObject& visibleObject);
	static bool hasRespawnTask(model::gameobjects::VisibleObject& visibleObject);
	static bool setAutoReleaseId(int32_t objectId);
	static void cancelRespawn(model::gameobjects::VisibleObject& object);
	/**
	 * Cancels the respawn for the given objectId only if it also matches the given spawn template (meaning, that this respawn belonged to an npc with
	 * the specified spawn template)
	 */
	static bool cancelRespawn(int32_t objectId, model::templates::spawns::SpawnTemplate& spawnTemplate);
	static int32_t cancelRespawns(const std::function<bool(model::templates::spawns::SpawnTemplate&)>& predicate);
	static int32_t cancelEventRespawns(const model::templates::event::EventTemplate* eventTemplate);
};

} // namespace aion::gameserver::services
