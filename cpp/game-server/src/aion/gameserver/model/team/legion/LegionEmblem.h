#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/team/legion/LegionEmblemType.h"
#include "aion/gameserver/model/team/legion/fwd.h"

namespace aion::gameserver::model::team::legion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Legion.legionEmblem`), created with create(). The byte
 * arrays are stored and returned as they are (runtime Array, null allowed: resetUploadSettings stores null, setEmblem checks for null).
 *
 * @author Simple, cura, Neon
 */
class LegionEmblem : public runtime::RefCounted, public gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int8_t> emblemId{0};
	runtime::Field<int8_t> color_a{0};
	runtime::Field<int8_t> color_r{0};
	runtime::Field<int8_t> color_g{0};
	runtime::Field<int8_t> color_b{0};
	runtime::Field<LegionEmblemType> emblemType{LegionEmblemType::DEFAULT};
	runtime::Field<PersistentState> persistentState; // Java: setPersistentState(PersistentState.NEW) (constructor)
	runtime::Field<bool> isUploading_{false};
	runtime::Field<int32_t> uploadSize{0};
	runtime::Field<int32_t> uploadedSize{0};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> uploadData{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> customEmblemData; // Java: = {} (constructor)

protected:
	LegionEmblem();
	~LegionEmblem() override;

public:
	/** Java: new LegionEmblem() */
	static runtime::Ref<LegionEmblem> create();

	runtime::Ptr<runtime::Array<int8_t>> getCustomEmblemData() const { return customEmblemData.get(); }

	void setCustomEmblemData(runtime::Ptr<runtime::Array<int8_t>> customEmblemData);

	void setEmblem(int32_t emblemId, int32_t color_a, int32_t color_r, int32_t color_g, int32_t color_b, LegionEmblemType emblemType,
		runtime::Ptr<runtime::Array<int8_t>> emblem_data);

	int8_t getEmblemId() const { return emblemId.get(); }

	/** @return The alpha value. */
	int8_t getColor_a() const { return color_a.get(); }

	int8_t getColor_r() const { return color_r.get(); }

	int8_t getColor_g() const { return color_g.get(); }

	int8_t getColor_b() const { return color_b.get(); }

	void setUploading(bool value) { isUploading_.set(value); }

	bool isUploading() const { return isUploading_.get(); }

	void setUploadSize(int32_t emblemSize) { uploadSize.set(emblemSize); }

	int32_t getUploadSize() const { return uploadSize.get(); }

	void addUploadData(runtime::Ptr<runtime::Array<int8_t>> data);

	runtime::Ptr<runtime::Array<int8_t>> getUploadData() const { return uploadData.get(); }

	void addUploadedSize(int32_t uploadedSize);

	int32_t getUploadedSize() const { return uploadedSize.get(); }

	void setEmblemType(LegionEmblemType value) { emblemType.set(value); }

	LegionEmblemType getEmblemType() const { return emblemType.get(); }

	/** This method will clear out all upload data */
	void resetUploadSettings();

	void setPersistentState(PersistentState persistentState) override;

	PersistentState getPersistentState() override { return persistentState.get(); }
};

} // namespace aion::gameserver::model::team::legion
