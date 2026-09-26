#include "aion/gameserver/model/gameobjects/Letter.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"

namespace aion::gameserver::model::gameobjects {

Letter::Letter(int32_t objId, int32_t value, runtime::Ptr<Item> attachedItemValue, int64_t attachedKinahCountValue, std::string_view titleValue,
	std::string_view messageValue, std::string_view senderNameValue, std::optional<commons::database::Timestamp> timeStampValue, bool unreadValue,
	LetterType letterTypeValue)
	: AionObject(objId), recipientId(value), attachedItem(attachedItemValue), attachedKinahCount(attachedKinahCountValue),
	  senderName(std::string(senderNameValue)), title(std::string(titleValue)), message(std::string(messageValue)), unread(unreadValue),
	  express(letterTypeValue == LetterType::EXPRESS || letterTypeValue == LetterType::BLACKCLOUD),
	  // Java stores the Timestamp as given; every caller passes one (MailService/SystemMailService: now, MailDAO: the NOT NULL recieved_time column)
	  timeStamp(timeStampValue.value_or(commons::database::Timestamp{})), persistentState(PersistentState::NEW), letterType(letterTypeValue) {
}

runtime::Ref<Letter> Letter::create(int32_t objId, int32_t value, runtime::Ptr<Item> attachedItemValue, int64_t attachedKinahCountValue,
	std::string_view titleValue, std::string_view messageValue, std::string_view senderNameValue,
	std::optional<commons::database::Timestamp> timeStampValue, bool unreadValue, LetterType letterTypeValue) {
	return runtime::makeRef<Letter>(objId, value, attachedItemValue, attachedKinahCountValue, titleValue, messageValue, senderNameValue, timeStampValue,
		unreadValue, letterTypeValue);
}

void Letter::setAttachedItem(runtime::Ptr<Item> value) {
	attachedItem.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void Letter::setReadLetter() {
	unread.set(false);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void Letter::setExpress(bool value) {
	express.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void Letter::setLetterType(LetterType value) {
	letterType.set(value);
	if (value == LetterType::EXPRESS || value == LetterType::BLACKCLOUD)
		express.set(true);
	else
		express.set(false);
}

void Letter::removeAttachedKinah() {
	attachedKinahCount.set(0);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void Letter::setPersistentState(Persistable::PersistentState state) {
	persistentState.set(state);
}

Letter::~Letter() = default;

} // namespace aion::gameserver::model::gameobjects
