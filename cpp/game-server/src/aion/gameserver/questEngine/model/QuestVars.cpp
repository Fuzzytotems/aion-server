#include "aion/gameserver/questEngine/model/QuestVars.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::model {

// Java: LoggerFactory.getLogger(QuestVars.class), created in setVarById's body - the .cpp logger (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.model.QuestVars");

QuestVars::QuestVars() : questVars(runtime::Array<int32_t>::make(6)) {
}

QuestVars::QuestVars(int32_t var) : questVars(runtime::Array<int32_t>::make(6)) {
	setVar(var);
}

QuestVars::~QuestVars() = default;

runtime::Ref<QuestVars> QuestVars::create() {
	return runtime::makeRef<QuestVars>();
}

runtime::Ref<QuestVars> QuestVars::create(int32_t var) {
	return runtime::makeRef<QuestVars>(var);
}

int32_t QuestVars::getVarById(int32_t id) {
	return (*questVars)[id].get(); // Java questVars[id]: ArrayIndexOutOfBoundsException outside [0, 6), as runtime::Array throws
}

void QuestVars::setVarById(int32_t id, int32_t var) {
	if (var > 0x3F) // Java: warn(message, new IllegalArgumentException()) - the exception only carries the stack trace into the log
		log.warn("Out of range value was passed for quest var on index " + std::to_string(id), runtime::IllegalArgumentException(""));
	(*questVars)[id].set(var); // stored as given after the warning; an index outside [0, 6) throws only here, at Java's array store
}

int32_t QuestVars::getQuestVars() {
	// Java int arithmetic: `var <<= 6` drops the bits shifted out of the int and `var |= questVars[i]` ors the whole int, so a value above
	// 0x3F (stored with setVarById's warning) or a negative one spills into the bits of the variables above it, as in Java
	uint32_t var = 0;
	for (int32_t i = 5; i >= 0; i--) {
		var <<= 0x06;
		var |= static_cast<uint32_t>((*questVars)[i].get());
	}
	return static_cast<int32_t>(var);
}

void QuestVars::setVar(int32_t var) {
	// ported with the constructor (hub-headers.md §2: creatable objects); Java >>= on int is an arithmetic shift, as in C++20
	for (int32_t i = 0; i <= 5; i++) {
		(*questVars)[i].set(var & 0x3F);
		var >>= 0x06;
	}
}

} // namespace aion::gameserver::questEngine::model
