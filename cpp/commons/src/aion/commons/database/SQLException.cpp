#include "aion/commons/database/SQLException.h"

#include <algorithm>
#include <limits>

namespace aion::commons::database {

SQLException::SQLException(const std::string& reason, std::string sqlState, int32_t vendorCode, std::stacktrace trace)
	: Exception(reason, std::move(trace)), sqlState(std::move(sqlState)), vendorCode(vendorCode) {
}

SQLException::SQLException(const std::string& reason, std::exception_ptr cause, std::stacktrace trace)
	: Exception(reason, std::move(cause), std::move(trace)), vendorCode(0) {
}

SQLException::SQLException(const std::string& reason, std::string sqlState, int32_t vendorCode, std::exception_ptr cause, std::stacktrace trace)
	: Exception(reason, std::move(cause), std::move(trace)), sqlState(std::move(sqlState)), vendorCode(vendorCode) {
}

BatchUpdateException::BatchUpdateException(const std::string& reason, std::string sqlState, int32_t vendorCode, std::vector<int64_t> updateCounts,
	std::exception_ptr cause, std::stacktrace trace)
	: SQLException(reason, std::move(sqlState), vendorCode, std::move(cause), std::move(trace)), updateCounts(std::move(updateCounts)) {
}

std::vector<int32_t> BatchUpdateException::getUpdateCounts() const {
	std::vector<int32_t> counts;
	counts.reserve(updateCounts.size());
	for (int64_t count : updateCounts)
		counts.push_back(static_cast<int32_t>(std::clamp<int64_t>(count, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max())));
	return counts;
}

} // namespace aion::commons::database
