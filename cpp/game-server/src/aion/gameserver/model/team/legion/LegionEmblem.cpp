#include "aion/gameserver/model/team/legion/LegionEmblem.h"

namespace aion::gameserver::model::team::legion {

LegionEmblem::LegionEmblem() : persistentState(PersistentState::NEW), customEmblemData(runtime::Array<int8_t>::make(0)) {
	// Java: setPersistentState(PersistentState.NEW), which stores NEW into the unset state
}

LegionEmblem::~LegionEmblem() = default;

runtime::Ref<LegionEmblem> LegionEmblem::create() {
	return runtime::makeRef<LegionEmblem>();
}

void LegionEmblem::setCustomEmblemData(runtime::Ptr<runtime::Array<int8_t>> value) {
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	this->customEmblemData.set(runtime::Ref<runtime::Array<int8_t>>(value));
	this->emblemType.set(LegionEmblemType::CUSTOM);
}

void LegionEmblem::setEmblem(int32_t emblemIdValue, int32_t colorAValue, int32_t colorRValue, int32_t colorGValue, int32_t colorBValue,
	LegionEmblemType emblemTypeValue, runtime::Ptr<runtime::Array<int8_t>> emblem_data) {
	this->emblemId.set(static_cast<int8_t>(emblemIdValue));
	this->color_a.set(static_cast<int8_t>(colorAValue));
	this->color_r.set(static_cast<int8_t>(colorRValue));
	this->color_g.set(static_cast<int8_t>(colorGValue));
	this->color_b.set(static_cast<int8_t>(colorBValue));
	this->emblemType.set(emblemTypeValue);
	this->customEmblemData.set(runtime::Ref<runtime::Array<int8_t>>(emblem_data));
	if (this->emblemType.get() == LegionEmblemType::CUSTOM && !customEmblemData.get())
		this->emblemType.set(LegionEmblemType::DEFAULT);

	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void LegionEmblem::addUploadData(runtime::Ptr<runtime::Array<int8_t>> data) {
	// Java: new byte[uploadedSize] - the caller adds the chunk's size first (LegionService.uploadEmblemData); a chunk beyond it throws
	runtime::Ref<runtime::Array<int8_t>> newData = runtime::Array<int8_t>::make(uploadedSize.get());
	int32_t i = 0;
	runtime::Ptr<runtime::Array<int8_t>> previous = uploadData.get();
	if (previous && previous->length() > 0) {
		for (int8_t dataByte : *previous) {
			(*newData)[i] = dataByte;
			i++;
		}
	}
	for (int8_t dataByte : *data) {
		(*newData)[i] = dataByte;
		i++;
	}
	this->uploadData.set(std::move(newData));
}

void LegionEmblem::addUploadedSize(int32_t value) {
	// java-race: unsynchronized read-modify-write of uploadedSize (Java `this.uploadedSize += uploadedSize`)
	this->uploadedSize.set(this->uploadedSize.get() + value);
}

void LegionEmblem::resetUploadSettings() {
	this->isUploading_.set(false);
	this->uploadedSize.set(0);
	this->uploadData.set(nullptr);
}

void LegionEmblem::setPersistentState(PersistentState value) {
	switch (value) {
		case PersistentState::UPDATE_REQUIRED:
			if (this->persistentState.get() == PersistentState::NEW)
				break;
			[[fallthrough]];
		default:
			this->persistentState.set(value);
	}
}

} // namespace aion::gameserver::model::team::legion
