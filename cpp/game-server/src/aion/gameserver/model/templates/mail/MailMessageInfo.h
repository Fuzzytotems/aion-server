#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/mail/MailMessage.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Companion of the generated enum MailMessage (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`getId(message)` for Java `message.getId()`).
 *
 * @author kosyachok
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 7> MAIL_MESSAGE_IDS{
	0, // MAIL_SEND_SUCCESS
	1, // NO_SUCH_CHARACTER_NAME
	2, // RECIPIENT_MAILBOX_FULL
	3, // MAIL_IS_ONE_RACE_ONLY
	4, // YOU_ARE_IN_RECIPIENT_IGNORE_LIST
	5, // RECIPIENT_IGNORING_MAIL_FROM_PLAYERS_LOWER_206_LVL
	6, // MAILSPAM_WAIT_FOR_SOME_TIME
};
static_assert(static_cast<size_t>(MailMessage::MAILSPAM_WAIT_FOR_SOME_TIME) + 1 == MAIL_MESSAGE_IDS.size(), "one entry per MailMessage constant");
} // namespace detail

constexpr int32_t getId(MailMessage message) noexcept {
	return detail::MAIL_MESSAGE_IDS[static_cast<size_t>(message)];
}

} // namespace aion::gameserver::model::templates::mail
