#include "aion/gameserver/model/gameobjects/player/Mailbox.h"

#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

Mailbox::Mailbox(Player& player) : OwnedPart(player), owner(player) {
}

Mailbox::~Mailbox() = default;

void Mailbox::putLetterToMailbox(Letter& letter) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Letter>> Mailbox::getLetters() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Letter>> Mailbox::getNewSystemLetters(std::string_view substring) {
	AION_UNPORTED();
}

runtime::Ptr<Letter> Mailbox::getLetterFromMailbox(int32_t letterObjId) {
	AION_UNPORTED();
}

bool Mailbox::haveUnread() {
	AION_UNPORTED();
}

int32_t Mailbox::getUnreadCount() {
	AION_UNPORTED();
}

bool Mailbox::haveUnreadByType(LetterType letterType) {
	AION_UNPORTED();
}

int32_t Mailbox::getUnreadCountByType(LetterType letterType) {
	AION_UNPORTED();
}

bool Mailbox::haveFreeSlots() {
	AION_UNPORTED();
}

void Mailbox::removeLetter(int32_t letterId) {
	AION_UNPORTED();
}

int32_t Mailbox::size() {
	AION_UNPORTED();
}

void Mailbox::uploadReserveLetters() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
