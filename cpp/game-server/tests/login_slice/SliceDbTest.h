#pragma once

// Fixture of the database-backed login slice tests (P5-00): the test database of LoginSliceTestSupport.h, the slice's static data, a deterministic
// thread pool, a fresh IDFactory, the SlicePlayer factory and a task scope for the runtime references. TearDown closes the scope and drains the
// Reclaimer, so a test may count destroyed players after calling closeScopeAndDrain().

#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "../support/LoginSliceTestSupport.h"

namespace aion::gameserver::loginslice::test {

class SliceDbTest : public testing::Test {
protected:
	void SetUp() override {
		if (!isDatabaseEnabled())
			GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the login slice database tests";
		setUpDatabaseOnce();
		clearTables();
		applyConfigDefaultsOnce();
		publishStaticDataOnce();
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		factory.emplace();
		scope.emplace(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	void TearDown() override {
		if (!isDatabaseEnabled())
			return;
		closeScopeAndDrain();
		factory.reset();
		utils::ThreadPoolManager::installBackend(nullptr);
		drainReclaimer();
	}

	/** Ends the test's task scope (its borrows) and runs full Reclaimer scans; the Refs of the test must be released before */
	void closeScopeAndDrain() {
		scope.reset();
		drainReclaimer();
	}

	/** Opens a new task scope after closeScopeAndDrain() */
	void reopenScope() { scope.emplace(AION_TASK_INFO(runtime::TaskKind::TEST)); }

	runtime::ManualClock clock{0};
	std::optional<SlicePlayerFactoryScope> factory;
	std::optional<runtime::TaskScope> scope;
};

} // namespace aion::gameserver::loginslice::test
