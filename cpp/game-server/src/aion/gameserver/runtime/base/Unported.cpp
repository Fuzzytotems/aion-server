#include "aion/gameserver/runtime/base/Unported.h"

#include <algorithm>
#include <ostream>
#include <tuple>

#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::runtime {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.Unported"));
	return *logger;
}

/** head of the list of reached sites (push only, never unlinked: sites are statics) */
constinit std::atomic<UnportedSite*> sites{nullptr};

void publish(UnportedSite& site, const std::source_location& location) noexcept {
	site.location = location;
	UnportedSite* head = sites.load(std::memory_order_relaxed);
	do {
		site.next = head;
	} while (!sites.compare_exchange_weak(head, &site));
}

} // namespace

namespace detail {

std::string shortenUnportedFileName(std::string_view fileName) {
	std::string file(fileName);
	std::ranges::replace(file, '\\', '/');
	size_t position = file.rfind("/aion/gameserver/");
	if (position != std::string::npos)
		return file.substr(position + 1);
	return file;
}

} // namespace detail

void unportedReached(UnportedSite& site, std::source_location location) {
	site.hits.fetch_add(1, std::memory_order_relaxed);
	std::string file = detail::shortenUnportedFileName(location.file_name());
	std::string message = std::string(location.function_name()) + " is not ported yet (" + file + ":" + std::to_string(location.line()) + ")";
	UnportedException exception(message);
	if (!site.registered.exchange(true)) {
		publish(site, location);
		try {
			log().warn("AION_UNPORTED reached: " + std::string(location.function_name()) + " at " + file + ":" + std::to_string(location.line()),
				exception);
		} catch (...) {
			// logging must not replace the UnportedException
		}
	}
	throw exception;
}

std::vector<UnportedHit> unportedHits() {
	std::vector<UnportedHit> result;
	for (UnportedSite* site = sites.load(); site != nullptr; site = site->next) {
		result.push_back(UnportedHit{.function = site->location.function_name(),
			.file = detail::shortenUnportedFileName(site->location.file_name()),
			.line = site->location.line(),
			.hits = site->hits.load(std::memory_order_relaxed)});
	}
	std::ranges::sort(result, [](const UnportedHit& a, const UnportedHit& b) {
		return std::tie(a.file, a.line, a.function) < std::tie(b.file, b.line, b.function);
	});
	return result;
}

uint64_t unportedHitCount() noexcept {
	uint64_t total = 0;
	for (UnportedSite* site = sites.load(); site != nullptr; site = site->next)
		total += site->hits.load(std::memory_order_relaxed);
	return total;
}

void writeUnportedTrace(std::ostream& out) {
	out << "# AION_UNPORTED trace v1\n";
	for (const UnportedHit& hit : unportedHits())
		out << hit.hits << '\t' << hit.file << ':' << hit.line << '\t' << hit.function << '\n';
}

void resetUnportedHitsForTests() noexcept {
	for (UnportedSite* site = sites.load(); site != nullptr; site = site->next)
		site->hits.store(0, std::memory_order_relaxed);
}

} // namespace aion::gameserver::runtime
