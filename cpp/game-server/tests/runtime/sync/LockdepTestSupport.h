#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

// Lock-order validator integration for GoogleTest executables (design §4.2 "failOnReport makes CTest fail", conventions: "Lock-order
// validator reports fail tests"). Include this header in exactly one .cpp of a test executable and put
//   AION_LOCKDEP_FAIL_TESTS_ON_CYCLES();
// at namespace scope: it enables failOnReport and registers a listener that fails every test during which a new CYCLE report appeared.
// Tests that provoke inversions on purpose call LockOrderValidator::clearReports() afterwards, which resets failureCount().

namespace aion::gameserver::runtime::testsupport {

class LockdepFailureListener : public ::testing::EmptyTestEventListener {
public:
	void OnTestStart(const ::testing::TestInfo&) override { failuresAtStart = LockOrderValidator::getInstance().failureCount(); }

	void OnTestEnd(const ::testing::TestInfo& info) override {
		uint64_t failures = LockOrderValidator::getInstance().failureCount();
		if (failures > failuresAtStart) {
			std::string text;
			for (const LockOrderValidator::Report& report : LockOrderValidator::getInstance().getReports())
				if (report.kind == LockOrderValidator::ReportKind::CYCLE)
					text += report.text + "\n";
			ADD_FAILURE() << "lock-order validator reported " << (failures - failuresAtStart) << " cycle(s) during " << info.name() << ":\n" << text;
		}
	}

private:
	uint64_t failuresAtStart = 0;
};

/** Registers the listener (idempotent per call site). @return true */
inline bool installLockdepFailureListener() {
	LockOrderValidator::getInstance().setFailOnReport(true);
	::testing::UnitTest::GetInstance()->listeners().Append(new LockdepFailureListener());
	return true;
}

} // namespace aion::gameserver::runtime::testsupport

#define AION_LOCKDEP_FAIL_TESTS_ON_CYCLES()                                                                                                         \
	[[maybe_unused]] static const bool aionLockdepFailureListenerInstalled = ::aion::gameserver::runtime::testsupport::installLockdepFailureListener()
