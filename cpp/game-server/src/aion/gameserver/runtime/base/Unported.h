#pragma once

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Exception.h"

/**
 * AION_UNPORTED: the body of a declared but not yet ported function (docs/design/handlers-and-porting-plan.md §2.1 "everything links from day
 * one"). No Java counterpart.
 * <pre>
 * void QuestService::finishQuest(QuestEnv& env) {
 *     AION_UNPORTED();
 * }
 * int32_t Npc::getAggroRange() const {
 *     AION_UNPORTED(); // [[noreturn]]: no return statement needed
 * }
 * </pre>
 * Reaching it throws UnportedException ("<function> is not ported yet (<file>:<line>)"). The packet, task and command boundaries catch and log
 * it like any exception, so a running server shows which bodies a scenario still needs. In addition every site
 * - logs one warning with the stack trace the first time it is reached: "AION_UNPORTED reached: <function> at <file>:<line>";
 * - counts its hits; unportedHits() / writeUnportedTrace() list all sites reached so far (the input of tools/porting/unported_trace.py).
 * File names are shortened to the part from "aion/gameserver/" on (forward slashes), so traces do not depend on the checkout directory.
 * <p>
 * Thread-safe and allocation-free until a site is first reached. Do not use it in noexcept functions (the exception would terminate).
 * <p>
 * Placement: aion_gs_runtime_base (namespace aion::gameserver::runtime), so every core and handler library can use it without linking the
 * handler registry. Wave 1 had it in aion_gs_handler_registry (aion/gameserver/handlers/Unported.h); spine step S0a moved it here and kept the
 * macro, UnportedException and the trace API.
 */
namespace aion::gameserver::runtime {

/** Thrown by AION_UNPORTED. Derives from UnsupportedOperationException (Java code catching RuntimeException/Exception catches it too). */
class UnportedException : public commons::utils::UnsupportedOperationException {
public:
	using UnsupportedOperationException::UnsupportedOperationException;
};

/** The per-site state of one AION_UNPORTED expansion (a constinit function-local static). Internal to the macro. */
struct UnportedSite {
	std::atomic<uint64_t> hits{0};
	std::atomic<bool> registered{false};
	std::source_location location{}; // written once by the registering thread before the site is published
	UnportedSite* next = nullptr;     // published list, written before publication
};

/** One site of unportedHits() */
struct UnportedHit {
	std::string function; // compiler spelling of the function (std::source_location::function_name)
	std::string file;     // shortened, see above
	uint32_t line = 0;
	uint64_t hits = 0;
};

/** Counts the hit, logs the first one, throws UnportedException. Called by AION_UNPORTED; the location defaults to the macro's call site. */
[[noreturn]] void unportedReached(UnportedSite& site, std::source_location location = std::source_location::current());

/** @return every site reached since startup (or the last reset) with its hit count, sorted by file, line and function */
std::vector<UnportedHit> unportedHits();

/** @return the sum of all hits */
uint64_t unportedHitCount() noexcept;

/**
 * Writes the unported trace: a header line "# AION_UNPORTED trace v1" followed by one line per site from unportedHits(), tab separated:
 * <tt>hits file:line function</tt>.
 */
void writeUnportedTrace(std::ostream& out);

/** Sets all hit counts to 0 (sites stay listed with 0 hits and do not log again). For tests. */
void resetUnportedHitsForTests() noexcept;

namespace detail {
/** "D:\\x\\cpp\\game-server\\src\\aion\\gameserver\\world\\World.cpp" -> "aion/gameserver/world/World.cpp". Exposed for tests. */
std::string shortenUnportedFileName(std::string_view fileName);
} // namespace detail

} // namespace aion::gameserver::runtime

/**
 * The wave-1 names (namespace aion::gameserver::handlers), kept by S0a decision 1: handler code in aion::gameserver::handlers::* names
 * UnportedException unqualified, parity harnesses may spell handlers::unportedHitCount(). The same entities as in runtime, so code that sees
 * both namespaces is not ambiguous.
 */
namespace aion::gameserver::handlers {
using runtime::UnportedException;
using runtime::UnportedHit;
using runtime::unportedHitCount;
using runtime::unportedHits;
using runtime::resetUnportedHitsForTests;
using runtime::writeUnportedTrace;
} // namespace aion::gameserver::handlers

/**
 * Body of an unported function: throws UnportedException after counting the hit (see Unported.h). The lambda gives every expansion its own
 * site object; the source location is taken at the expansion, i.e. in the unported function itself.
 */
#define AION_UNPORTED()                                                                                                                               \
	::aion::gameserver::runtime::unportedReached([]() noexcept -> ::aion::gameserver::runtime::UnportedSite& {                                        \
		static constinit ::aion::gameserver::runtime::UnportedSite aionUnportedSite; /* lint: the site counter of the unported trace */               \
		return aionUnportedSite;                                                                                                                      \
	}())
