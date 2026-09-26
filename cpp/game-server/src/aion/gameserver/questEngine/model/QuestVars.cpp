#include "aion/gameserver/questEngine/model/QuestVars.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::questEngine::model {

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
	AION_UNPORTED();
}

void QuestVars::setVarById(int32_t id, int32_t var) {
	// Java: a logger created in the body (LoggerFactory.getLogger(QuestVars.class).warn(...)) is the .cpp logger (hub-headers.md §11.3)
	AION_UNPORTED();
}

int32_t QuestVars::getQuestVars() {
	AION_UNPORTED();
}

void QuestVars::setVar(int32_t var) {
	// ported with the constructor (hub-headers.md §2: creatable objects); Java >>= on int is an arithmetic shift, as in C++20
	for (int32_t i = 0; i <= 5; i++) {
		(*questVars)[i].set(var & 0x3F);
		var >>= 0x06;
	}
}

} // namespace aion::gameserver::questEngine::model
