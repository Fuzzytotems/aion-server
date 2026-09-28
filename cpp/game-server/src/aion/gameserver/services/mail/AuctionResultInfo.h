#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/services/mail/AuctionResult.h"

namespace aion::gameserver::services::mail {

/** Companion of the generated enum AuctionResult (docs/design/static-data.md §2.5): Java's constructor data and lookup as free functions (ADL). */

/** Java: AuctionResult.getId() - FAILED_BID(0) ... GRACE_SUCCESS(7), equal to the ordinal */
constexpr int32_t getId(AuctionResult result) noexcept {
	return static_cast<int32_t>(result);
}

/** Java: AuctionResult.getResultFromId(int) - std::nullopt (Java null) for an unknown id */
constexpr std::optional<AuctionResult> auctionResultOf(int32_t resultId) noexcept {
	if (resultId < 0 || resultId > getId(AuctionResult::GRACE_SUCCESS))
		return std::nullopt;
	return static_cast<AuctionResult>(resultId);
}

} // namespace aion::gameserver::services::mail
