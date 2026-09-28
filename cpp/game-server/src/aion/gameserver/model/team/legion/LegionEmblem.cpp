#include "aion/gameserver/model/team/legion/LegionEmblem.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::team::legion {

LegionEmblem::LegionEmblem() : persistentState(PersistentState::NEW), customEmblemData(runtime::Array<int8_t>::make(0)) {
	// Java: setPersistentState(PersistentState.NEW), which stores NEW into the unset state
}

LegionEmblem::~LegionEmblem() = default;

runtime::Ref<LegionEmblem> LegionEmblem::create() {
	return runtime::makeRef<LegionEmblem>();
}

void LegionEmblem::setCustomEmblemData(runtime::Ptr<runtime::Array<int8_t>> value) {
	AION_UNPORTED();
}

void LegionEmblem::setEmblem(int32_t emblemIdValue, int32_t colorAValue, int32_t colorRValue, int32_t colorGValue, int32_t colorBValue,
	LegionEmblemType emblemTypeValue, runtime::Ptr<runtime::Array<int8_t>> emblem_data) {
	AION_UNPORTED();
}

void LegionEmblem::addUploadData(runtime::Ptr<runtime::Array<int8_t>> data) {
	AION_UNPORTED();
}

void LegionEmblem::addUploadedSize(int32_t value) {
	AION_UNPORTED();
}

void LegionEmblem::resetUploadSettings() {
	AION_UNPORTED();
}

void LegionEmblem::setPersistentState(PersistentState value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::legion
