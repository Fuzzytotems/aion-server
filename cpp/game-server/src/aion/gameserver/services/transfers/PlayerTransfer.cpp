#include "aion/gameserver/services/transfers/PlayerTransfer.h"

#include <iterator>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::services::transfers {

PlayerTransfer::PlayerTransfer(int32_t value, int32_t targetAccountValue, std::string_view accountValue, std::string_view nameValue)
	: taskId(value), targetAccount(targetAccountValue), name(std::string(nameValue)), account(std::string(accountValue)) {
}

runtime::Ref<PlayerTransfer> PlayerTransfer::create(int32_t value, int32_t targetAccountValue, std::string_view accountValue,
	std::string_view nameValue) {
	return runtime::makeRef<PlayerTransfer>(value, targetAccountValue, accountValue, nameValue);
}

void PlayerTransfer::setItemsData(runtime::Ptr<runtime::Array<int8_t>> value) {
	itemsData.set(value);
}

void PlayerTransfer::setCommonData(runtime::Ptr<runtime::Array<int8_t>> value) {
	commonData.set(value);
}

void PlayerTransfer::setSkillData(runtime::Ptr<runtime::Array<int8_t>> value) {
	skillData.set(value);
}

void PlayerTransfer::setRecipeData(runtime::Ptr<runtime::Array<int8_t>> value) {
	recipeData.set(value);
}

void PlayerTransfer::setQuestData(runtime::Ptr<runtime::Array<int8_t>> value) {
	questData.set(value);
}

void PlayerTransfer::setData(runtime::Ptr<runtime::Array<int8_t>> value) {
	data.set(value);
}

// Java PlayerTransfer.java:85-97: the six arrays in this order (getX().length of a null array: NullPointerException); the C++ returns the
// bytes, which CMT_CHARACTER_INFORMATION wraps into its little-endian buffer
std::vector<uint8_t> PlayerTransfer::getDB() {
	const runtime::Ptr<runtime::Array<int8_t>> parts[] = {getCommonData(), getItemsData(), getData(), getSkillData(), getRecipeData(), getQuestData()};
	const char* const names[] = {"getCommonData()", "getItemsData()", "getData()", "getSkillData()", "getRecipeData()", "getQuestData()"};
	std::vector<uint8_t> buffer;
	for (size_t i = 0; i < std::size(parts); ++i) {
		if (parts[i] == nullptr)
			throw runtime::NullPointerException(names[i]);
	}
	for (const runtime::Ptr<runtime::Array<int8_t>>& part : parts)
		for (int8_t b : part->snapshot())
			buffer.push_back(static_cast<uint8_t>(b));
	return buffer;
}

PlayerTransfer::~PlayerTransfer() = default;

} // namespace aion::gameserver::services::transfers
