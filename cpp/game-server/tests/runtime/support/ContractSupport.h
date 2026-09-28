#pragma once

#include <gtest/gtest.h>

// Shared helpers of the kernel contract tests (headers stage). PCT helpers for kernel tests are in PctSupport.h. Owned by the lifetime agent;
// other areas may add their own helper headers here.

namespace aion::gameserver::runtime::testsupport {

/** Not const, so the compiler cannot prove the code after AION_SKIP_UNTIL_IMPLEMENTED unreachable (C4702). */
inline bool contractStubsActive = true;

} // namespace aion::gameserver::runtime::testsupport

/**
 * Skips a contract test whose area is still a stub. Implementation agents delete the macro line from their tests once the area is implemented.
 */
#define AION_SKIP_UNTIL_IMPLEMENTED(reason)                                                                                                         \
	if (::aion::gameserver::runtime::testsupport::contractStubsActive)                                                                                \
	GTEST_SKIP() << reason
