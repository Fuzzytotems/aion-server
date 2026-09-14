#include "aion/gameserver/model/gameobjects/Letter.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::model::gameobjects {

Letter::Letter(int32_t objId, int32_t value, runtime::Ptr<Item> attachedItemValue, int64_t attachedKinahCountValue, std::string_view titleValue,
	std::string_view messageValue, std::string_view senderNameValue, std::optional<commons::database::Timestamp> timeStampValue, bool unreadValue,
	LetterType letterTypeValue)
	: AionObject(objId), recipientId(value), attachedItem(attachedItemValue), attachedKinahCount(attachedKinahCountValue),
	  senderName(std::string(senderNameValue)), title(std::string(titleValue)), message(std::string(messageValue)), unread(unreadValue),
	  letterType(letterTypeValue) {
	// Java: this.express = letterType == LetterType.EXPRESS || letterType == LetterType.BLACKCLOUD; this.timeStamp = timeStamp; this.persistentState =
	// PersistentState.NEW
	AION_UNPORTED();
}

runtime::Ref<Letter> Letter::create(int32_t objId, int32_t value, runtime::Ptr<Item> attachedItemValue, int64_t attachedKinahCountValue,
	std::string_view titleValue, std::string_view messageValue, std::string_view senderNameValue,
	std::optional<commons::database::Timestamp> timeStampValue, bool unreadValue, LetterType letterTypeValue) {
	return runtime::makeRef<Letter>(objId, value, attachedItemValue, attachedKinahCountValue, titleValue, messageValue, senderNameValue, timeStampValue,
		unreadValue, letterTypeValue);
}

void Letter::setAttachedItem(runtime::Ptr<Item> value) {
	AION_UNPORTED();
}

void Letter::setReadLetter() {
	AION_UNPORTED();
}

void Letter::setExpress(bool value) {
	AION_UNPORTED();
}

void Letter::setLetterType(LetterType value) {
	AION_UNPORTED();
}

void Letter::removeAttachedKinah() {
	AION_UNPORTED();
}

void Letter::setPersistentState(Persistable::PersistentState state) {
	persistentState.set(state);
}

Letter::~Letter() = default;

} // namespace aion::gameserver::model::gameobjects
