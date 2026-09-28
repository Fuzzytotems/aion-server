#pragma once

#include <cstdint>

namespace aion::gameserver::services::player {

/**
 * The states of Mailbox.mailBoxState (a Java byte): what SM_DIALOG_WINDOW writes as the mailbox state of the postbox page (DialogPage.MAIL).
 * <p>
 * C++: a class of constants like the Java class (a static-only class, hub-headers.md §11.1).
 *
 * @author Source
 */
class PlayerMailboxState {
public:
	static constexpr int8_t CLOSED = 0x00;
	static constexpr int8_t REGULAR = 0x01;
	static constexpr int8_t EXPRESS = 0x02;
};

} // namespace aion::gameserver::services::player
