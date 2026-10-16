#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ATracer
 */
class PeriodicSaveService : public runtime::Immortal {
private:
	class LegionWarehouseSaveTask;
	class ServerRunTimeSaveTask;
	class PeriodicSaveTask;
	// Java implements Runnable
	class PeriodicSaveTask : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const runtime::FutureRef future{};
	private:
		/**
		 * C++ only: false until `postConstruct()` runs. The Java constructor schedules `this`, and a tick that fires before the subclass
		 * constructor returned dispatches to the subclass override there; in C++ it would call the still pure virtual `run()` (undefined
		 * behaviour). The scheduled body therefore checks this flag (docs/deviations/P5-14.md, next to the `AbstractCronTask` row).
		 */
		// lint: L14 a construction flag, not game state: a Field<> would itself need the object to be constructed and a task scope to be read
		std::atomic<bool> constructed{false};
	protected:
		explicit PeriodicSaveTask(int64_t periodMillis);
		/** C++ only: called by the subclass `create()` once the object is fully constructed; from then on a tick runs `run()`. */
		void postConstruct() noexcept;
	private:
		/** The body the constructor schedules: `run()` once `postConstruct()` has run. */
		void runWhenConstructed();
	public:
		/** Java Runnable.run (abstract) */
		virtual void run() = 0;
		void storeDataAndCancel();
	protected:
		~PeriodicSaveTask() override;
	};
	class LegionWarehouseSaveTask : public PeriodicSaveService::PeriodicSaveTask {
		AION_MAKE_REF_FRIEND
	protected:
		LegionWarehouseSaveTask();
	public:
		static runtime::Ref<PeriodicSaveService::LegionWarehouseSaveTask> create();
		void run() override;
	protected:
		~LegionWarehouseSaveTask() override;
	};
	class ServerRunTimeSaveTask : public PeriodicSaveService::PeriodicSaveTask {
		AION_MAKE_REF_FRIEND
	protected:
		ServerRunTimeSaveTask();
	public:
		static runtime::Ref<PeriodicSaveService::ServerRunTimeSaveTask> create();
		void run() override;
	protected:
		~ServerRunTimeSaveTask() override;
	};
	runtime::ArrayList<runtime::Ref<PeriodicSaveService::PeriodicSaveTask>> tasks{AION_LOCK_CLASS(PeriodicSaveService::tasks)};
public:
	static PeriodicSaveService& getInstance(); // Java singleton
private:
	PeriodicSaveService();
public:
	/** Save data on shutdown */
	void onShutdown();
};

} // namespace aion::gameserver::services
