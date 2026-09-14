#include "aion/gameserver/geoEngine/math/Matrix3f.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::math {

namespace {

const commons::logging::Logger& logger() {
	static const auto* l = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.math.Matrix3f"));
	return *l;
}

} // namespace

void Matrix3f::absoluteLocal() noexcept {
	m00 = FastMath::abs(m00);
	m01 = FastMath::abs(m01);
	m02 = FastMath::abs(m02);
	m10 = FastMath::abs(m10);
	m11 = FastMath::abs(m11);
	m12 = FastMath::abs(m12);
	m20 = FastMath::abs(m20);
	m21 = FastMath::abs(m21);
	m22 = FastMath::abs(m22);
}

Matrix3f& Matrix3f::set(const Matrix3f& matrix) noexcept {
	m00 = matrix.m00;
	m01 = matrix.m01;
	m02 = matrix.m02;
	m10 = matrix.m10;
	m11 = matrix.m11;
	m12 = matrix.m12;
	m20 = matrix.m20;
	m21 = matrix.m21;
	m22 = matrix.m22;
	return *this;
}

float Matrix3f::get(int32_t i, int32_t j) const {
	switch (i) {
		case 0:
			switch (j) {
				case 0:
					return m00;
				case 1:
					return m01;
				case 2:
					return m02;
			}
			break;
		case 1:
			switch (j) {
				case 0:
					return m10;
				case 1:
					return m11;
				case 2:
					return m12;
			}
			break;
		case 2:
			switch (j) {
				case 0:
					return m20;
				case 1:
					return m21;
				case 2:
					return m22;
			}
	}
	logger().warn("Invalid matrix index.");
	throw commons::utils::IllegalArgumentException("Invalid indices into matrix.");
}

void Matrix3f::get(std::span<float> data, bool rowMajor) const {
	if (data.size() == 9) {
		if (rowMajor) {
			data[0] = m00;
			data[1] = m01;
			data[2] = m02;
			data[3] = m10;
			data[4] = m11;
			data[5] = m12;
			data[6] = m20;
			data[7] = m21;
			data[8] = m22;
		} else {
			data[0] = m00;
			data[1] = m10;
			data[2] = m20;
			data[3] = m01;
			data[4] = m11;
			data[5] = m21;
			data[6] = m02;
			data[7] = m12;
			data[8] = m22;
		}
	} else if (data.size() == 16) {
		if (rowMajor) {
			data[0] = m00;
			data[1] = m01;
			data[2] = m02;
			data[4] = m10;
			data[5] = m11;
			data[6] = m12;
			data[8] = m20;
			data[9] = m21;
			data[10] = m22;
		} else {
			data[0] = m00;
			data[1] = m10;
			data[2] = m20;
			data[4] = m01;
			data[5] = m11;
			data[6] = m21;
			data[8] = m02;
			data[9] = m12;
			data[10] = m22;
		}
	} else {
		throw commons::utils::IndexOutOfBoundsException("Array size must be 9 or 16 in Matrix3f.get().");
	}
}

Vector3f Matrix3f::getColumn(int32_t i) const {
	Vector3f store;
	getColumn(i, store);
	return store;
}

Vector3f& Matrix3f::getColumn(int32_t i, Vector3f& store) const {
	switch (i) {
		case 0:
			store.x = m00;
			store.y = m10;
			store.z = m20;
			break;
		case 1:
			store.x = m01;
			store.y = m11;
			store.z = m21;
			break;
		case 2:
			store.x = m02;
			store.y = m12;
			store.z = m22;
			break;
		default:
			logger().warn("Invalid column index.");
			throw commons::utils::IllegalArgumentException("Invalid column index. " + std::to_string(i));
	}
	return store;
}

Vector3f Matrix3f::getRow(int32_t i) const {
	Vector3f store;
	getRow(i, store);
	return store;
}

Vector3f& Matrix3f::getRow(int32_t i, Vector3f& store) const {
	switch (i) {
		case 0:
			store.x = m00;
			store.y = m01;
			store.z = m02;
			break;
		case 1:
			store.x = m10;
			store.y = m11;
			store.z = m12;
			break;
		case 2:
			store.x = m20;
			store.y = m21;
			store.z = m22;
			break;
		default:
			logger().warn("Invalid row index.");
			throw commons::utils::IllegalArgumentException("Invalid row index. " + std::to_string(i));
	}
	return store;
}

Matrix3f& Matrix3f::setColumn(int32_t i, const Vector3f& column) {
	switch (i) {
		case 0:
			m00 = column.x;
			m10 = column.y;
			m20 = column.z;
			break;
		case 1:
			m01 = column.x;
			m11 = column.y;
			m21 = column.z;
			break;
		case 2:
			m02 = column.x;
			m12 = column.y;
			m22 = column.z;
			break;
		default:
			logger().warn("Invalid column index.");
			throw commons::utils::IllegalArgumentException("Invalid column index. " + std::to_string(i));
	}
	return *this;
}

Matrix3f& Matrix3f::setRow(int32_t i, const Vector3f& row) {
	switch (i) {
		case 0:
			m00 = row.x;
			m01 = row.y;
			m02 = row.z;
			break;
		case 1:
			m10 = row.x;
			m11 = row.y;
			m12 = row.z;
			break;
		case 2:
			m20 = row.x;
			m21 = row.y;
			m22 = row.z;
			break;
		default:
			logger().warn("Invalid row index.");
			throw commons::utils::IllegalArgumentException("Invalid row index. " + std::to_string(i));
	}
	return *this;
}

Matrix3f& Matrix3f::set(int32_t i, int32_t j, float value) {
	switch (i) {
		case 0:
			switch (j) {
				case 0:
					m00 = value;
					return *this;
				case 1:
					m01 = value;
					return *this;
				case 2:
					m02 = value;
					return *this;
			}
			break;
		case 1:
			switch (j) {
				case 0:
					m10 = value;
					return *this;
				case 1:
					m11 = value;
					return *this;
				case 2:
					m12 = value;
					return *this;
			}
			break;
		case 2:
			switch (j) {
				case 0:
					m20 = value;
					return *this;
				case 1:
					m21 = value;
					return *this;
				case 2:
					m22 = value;
					return *this;
			}
	}
	logger().warn("Invalid matrix index.");
	throw commons::utils::IllegalArgumentException("Invalid indices into matrix.");
}

Matrix3f& Matrix3f::set(const std::array<std::array<float, 3>, 3>& matrix) noexcept {
	m00 = matrix[0][0];
	m01 = matrix[0][1];
	m02 = matrix[0][2];
	m10 = matrix[1][0];
	m11 = matrix[1][1];
	m12 = matrix[1][2];
	m20 = matrix[2][0];
	m21 = matrix[2][1];
	m22 = matrix[2][2];
	return *this;
}

void Matrix3f::fromAxes(const Vector3f& uAxis, const Vector3f& vAxis, const Vector3f& wAxis) noexcept {
	m00 = uAxis.x;
	m10 = uAxis.y;
	m20 = uAxis.z;

	m01 = vAxis.x;
	m11 = vAxis.y;
	m21 = vAxis.z;

	m02 = wAxis.x;
	m12 = wAxis.y;
	m22 = wAxis.z;
}

Matrix3f& Matrix3f::set(std::span<const float> matrix, bool rowMajor) {
	if (matrix.size() != 9)
		throw commons::utils::IllegalArgumentException("Array must be of size 9.");

	if (rowMajor) {
		m00 = matrix[0];
		m01 = matrix[1];
		m02 = matrix[2];
		m10 = matrix[3];
		m11 = matrix[4];
		m12 = matrix[5];
		m20 = matrix[6];
		m21 = matrix[7];
		m22 = matrix[8];
	} else {
		m00 = matrix[0];
		m01 = matrix[3];
		m02 = matrix[6];
		m10 = matrix[1];
		m11 = matrix[4];
		m12 = matrix[7];
		m20 = matrix[2];
		m21 = matrix[5];
		m22 = matrix[8];
	}
	return *this;
}

void Matrix3f::loadIdentity() noexcept {
	m01 = m02 = m10 = m12 = m20 = m21 = 0;
	m00 = m11 = m22 = 1;
}

bool Matrix3f::isIdentity() const noexcept {
	return (m00 == 1 && m01 == 0 && m02 == 0) && (m10 == 0 && m11 == 1 && m12 == 0) && (m20 == 0 && m21 == 0 && m22 == 1);
}

void Matrix3f::fromAngleAxis(float angle, const Vector3f& axis) noexcept {
	const Vector3f normAxis = axis.normalize();
	fromAngleNormalAxis(angle, normAxis);
}

void Matrix3f::fromAngleNormalAxis(float angle, const Vector3f& axis) noexcept {
	const float fCos = FastMath::cos(angle);
	const float fSin = FastMath::sin(angle);
	const float fOneMinusCos = 1.0f - fCos;
	const float fX2 = axis.x * axis.x;
	const float fY2 = axis.y * axis.y;
	const float fZ2 = axis.z * axis.z;
	const float fXYM = axis.x * axis.y * fOneMinusCos;
	const float fXZM = axis.x * axis.z * fOneMinusCos;
	const float fYZM = axis.y * axis.z * fOneMinusCos;
	const float fXSin = axis.x * fSin;
	const float fYSin = axis.y * fSin;
	const float fZSin = axis.z * fSin;

	m00 = fX2 * fOneMinusCos + fCos;
	m01 = fXYM - fZSin;
	m02 = fXZM + fYSin;
	m10 = fXYM + fZSin;
	m11 = fY2 * fOneMinusCos + fCos;
	m12 = fYZM - fXSin;
	m20 = fXZM - fYSin;
	m21 = fYZM + fXSin;
	m22 = fZ2 * fOneMinusCos + fCos;
}

Matrix3f Matrix3f::mult(const Matrix3f& mat) const noexcept {
	Matrix3f product;
	mult(mat, product);
	return product;
}

Matrix3f& Matrix3f::mult(const Matrix3f& mat, Matrix3f& product) const noexcept {
	float temp00, temp01, temp02;
	float temp10, temp11, temp12;
	float temp20, temp21, temp22;

	temp00 = m00 * mat.m00 + m01 * mat.m10 + m02 * mat.m20;
	temp01 = m00 * mat.m01 + m01 * mat.m11 + m02 * mat.m21;
	temp02 = m00 * mat.m02 + m01 * mat.m12 + m02 * mat.m22;
	temp10 = m10 * mat.m00 + m11 * mat.m10 + m12 * mat.m20;
	temp11 = m10 * mat.m01 + m11 * mat.m11 + m12 * mat.m21;
	temp12 = m10 * mat.m02 + m11 * mat.m12 + m12 * mat.m22;
	temp20 = m20 * mat.m00 + m21 * mat.m10 + m22 * mat.m20;
	temp21 = m20 * mat.m01 + m21 * mat.m11 + m22 * mat.m21;
	temp22 = m20 * mat.m02 + m21 * mat.m12 + m22 * mat.m22;

	product.m00 = temp00;
	product.m01 = temp01;
	product.m02 = temp02;
	product.m10 = temp10;
	product.m11 = temp11;
	product.m12 = temp12;
	product.m20 = temp20;
	product.m21 = temp21;
	product.m22 = temp22;

	return product;
}

Vector3f Matrix3f::mult(const Vector3f& vec) const noexcept {
	Vector3f product;
	mult(vec, product);
	return product;
}

Vector3f& Matrix3f::mult(const Vector3f& vec, Vector3f& product) const noexcept {
	const float x = vec.x;
	const float y = vec.y;
	const float z = vec.z;

	product.x = m00 * x + m01 * y + m02 * z;
	product.y = m10 * x + m11 * y + m12 * z;
	product.z = m20 * x + m21 * y + m22 * z;
	return product;
}

Matrix3f& Matrix3f::multLocal(float scale) noexcept {
	m00 *= scale;
	m01 *= scale;
	m02 *= scale;
	m10 *= scale;
	m11 *= scale;
	m12 *= scale;
	m20 *= scale;
	m21 *= scale;
	m22 *= scale;
	return *this;
}

Vector3f& Matrix3f::multLocal(Vector3f& vec) const noexcept {
	const float x = vec.x;
	const float y = vec.y;
	vec.x = m00 * x + m01 * y + m02 * vec.z;
	vec.y = m10 * x + m11 * y + m12 * vec.z;
	vec.z = m20 * x + m21 * y + m22 * vec.z;
	return vec;
}

Matrix3f& Matrix3f::multLocal(const Matrix3f& mat) noexcept {
	return mult(mat, *this);
}

Matrix3f& Matrix3f::transposeLocal() noexcept {
	float tmp = m01;
	m01 = m10;
	m10 = tmp;

	tmp = m02;
	m02 = m20;
	m20 = tmp;

	tmp = m12;
	m12 = m21;
	m21 = tmp;

	return *this;
}

Matrix3f Matrix3f::invert() const noexcept {
	Matrix3f store;
	invert(store);
	return store;
}

Matrix3f& Matrix3f::invert(Matrix3f& store) const noexcept {
	const float det = determinant();
	if (FastMath::abs(det) <= FastMath::FLOAT_EPSILON)
		return store.zero();

	store.m00 = m11 * m22 - m12 * m21;
	store.m01 = m02 * m21 - m01 * m22;
	store.m02 = m01 * m12 - m02 * m11;
	store.m10 = m12 * m20 - m10 * m22;
	store.m11 = m00 * m22 - m02 * m20;
	store.m12 = m02 * m10 - m00 * m12;
	store.m20 = m10 * m21 - m11 * m20;
	store.m21 = m01 * m20 - m00 * m21;
	store.m22 = m00 * m11 - m01 * m10;

	store.multLocal(1.0f / det);
	return store;
}

Matrix3f& Matrix3f::invertLocal() noexcept {
	const float det = determinant();
	if (FastMath::abs(det) <= FastMath::FLOAT_EPSILON)
		return zero();

	const float f00 = m11 * m22 - m12 * m21;
	const float f01 = m02 * m21 - m01 * m22;
	const float f02 = m01 * m12 - m02 * m11;
	const float f10 = m12 * m20 - m10 * m22;
	const float f11 = m00 * m22 - m02 * m20;
	const float f12 = m02 * m10 - m00 * m12;
	const float f20 = m10 * m21 - m11 * m20;
	const float f21 = m01 * m20 - m00 * m21;
	const float f22 = m00 * m11 - m01 * m10;

	m00 = f00;
	m01 = f01;
	m02 = f02;
	m10 = f10;
	m11 = f11;
	m12 = f12;
	m20 = f20;
	m21 = f21;
	m22 = f22;

	multLocal(1.0f / det);
	return *this;
}

Matrix3f Matrix3f::adjoint() const noexcept {
	Matrix3f store;
	adjoint(store);
	return store;
}

Matrix3f& Matrix3f::adjoint(Matrix3f& store) const noexcept {
	store.m00 = m11 * m22 - m12 * m21;
	store.m01 = m02 * m21 - m01 * m22;
	store.m02 = m01 * m12 - m02 * m11;
	store.m10 = m12 * m20 - m10 * m22;
	store.m11 = m00 * m22 - m02 * m20;
	store.m12 = m02 * m10 - m00 * m12;
	store.m20 = m10 * m21 - m11 * m20;
	store.m21 = m01 * m20 - m00 * m21;
	store.m22 = m00 * m11 - m01 * m10;
	return store;
}

float Matrix3f::determinant() const noexcept {
	const float fCo00 = m11 * m22 - m12 * m21;
	const float fCo10 = m12 * m20 - m10 * m22;
	const float fCo20 = m10 * m21 - m11 * m20;
	const float fDet = m00 * fCo00 + m01 * fCo10 + m02 * fCo20;
	return fDet;
}

Matrix3f& Matrix3f::zero() noexcept {
	m00 = m01 = m02 = m10 = m11 = m12 = m20 = m21 = m22 = 0.0f;
	return *this;
}

void Matrix3f::add(const Matrix3f& mat) noexcept {
	m00 += mat.m00;
	m01 += mat.m01;
	m02 += mat.m02;
	m10 += mat.m10;
	m11 += mat.m11;
	m12 += mat.m12;
	m20 += mat.m20;
	m21 += mat.m21;
	m22 += mat.m22;
}

Matrix3f Matrix3f::transposeNew() const noexcept {
	return Matrix3f(m00, m10, m20, m01, m11, m21, m02, m12, m22);
}

std::string Matrix3f::toString() const {
	std::string result = "Matrix3f\n[\n";
	result += " " + JavaFloat::toString(m00) + "  " + JavaFloat::toString(m01) + "  " + JavaFloat::toString(m02) + " \n";
	result += " " + JavaFloat::toString(m10) + "  " + JavaFloat::toString(m11) + "  " + JavaFloat::toString(m12) + " \n";
	result += " " + JavaFloat::toString(m20) + "  " + JavaFloat::toString(m21) + "  " + JavaFloat::toString(m22) + " \n]";
	return result;
}

int32_t Matrix3f::hashCode() const noexcept {
	// Java int arithmetic wraps: computed in uint32_t
	uint32_t hash = 37;
	for (float value : {m00, m01, m02, m10, m11, m12, m20, m21, m22})
		hash = 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(value));
	return static_cast<int32_t>(hash);
}

bool Matrix3f::equals(const Matrix3f& o) const noexcept {
	if (this == &o)
		return true;
	return JavaFloat::compare(m00, o.m00) == 0 && JavaFloat::compare(m01, o.m01) == 0 && JavaFloat::compare(m02, o.m02) == 0 &&
	       JavaFloat::compare(m10, o.m10) == 0 && JavaFloat::compare(m11, o.m11) == 0 && JavaFloat::compare(m12, o.m12) == 0 &&
	       JavaFloat::compare(m20, o.m20) == 0 && JavaFloat::compare(m21, o.m21) == 0 && JavaFloat::compare(m22, o.m22) == 0;
}

void Matrix3f::fromStartEndVectors(const Vector3f& start, const Vector3f& end) noexcept {
	Vector3f v;
	float e, h, f;

	start.cross(end, v);
	e = start.dot(end);
	f = (e < 0) ? -e : e;

	// if "from" and "to" vectors are nearly parallel
	if (f > 1.0f - FastMath::ZERO_TOLERANCE) {
		Vector3f u;
		Vector3f x;
		float c1, c2, c3; // coefficients for later use

		x.x = (start.x > 0.0) ? start.x : -start.x;
		x.y = (start.y > 0.0) ? start.y : -start.y;
		x.z = (start.z > 0.0) ? start.z : -start.z;

		if (x.x < x.y) {
			if (x.x < x.z) {
				x.x = 1.0f;
				x.y = x.z = 0.0f;
			} else {
				x.z = 1.0f;
				x.x = x.y = 0.0f;
			}
		} else {
			if (x.y < x.z) {
				x.y = 1.0f;
				x.x = x.z = 0.0f;
			} else {
				x.z = 1.0f;
				x.x = x.y = 0.0f;
			}
		}

		u.x = x.x - start.x;
		u.y = x.y - start.y;
		u.z = x.z - start.z;
		v.x = x.x - end.x;
		v.y = x.y - end.y;
		v.z = x.z - end.z;

		c1 = 2.0f / u.dot(u);
		c2 = 2.0f / v.dot(v);
		c3 = c1 * c2 * u.dot(v);

		for (int32_t i = 0; i < 3; i++) {
			for (int32_t j = 0; j < 3; j++) {
				const float val = -c1 * u.get(i) * u.get(j) - c2 * v.get(i) * v.get(j) + c3 * v.get(i) * u.get(j);
				set(i, j, val);
			}
			const float val = get(i, i);
			set(i, i, val + 1.0f);
		}
	} else {
		// the most common case, unless "start"="end", or "start"=-"end"
		float hvx, hvz, hvxy, hvxz, hvyz;
		h = 1.0f / (1.0f + e);
		hvx = h * v.x;
		hvz = h * v.z;
		hvxy = hvx * v.y;
		hvxz = hvx * v.z;
		hvyz = hvz * v.y;
		set(0, 0, e + hvx * v.x);
		set(0, 1, hvxy - v.z);
		set(0, 2, hvxz + v.y);

		set(1, 0, hvxy + v.z);
		set(1, 1, e + h * v.y * v.y);
		set(1, 2, hvyz - v.x);

		set(2, 0, hvxz - v.y);
		set(2, 1, hvyz + v.x);
		set(2, 2, e + hvz * v.z);
	}
}

void Matrix3f::scale(const Vector3f& scaleVector) noexcept {
	m00 *= scaleVector.x;
	m10 *= scaleVector.x;
	m20 *= scaleVector.x;
	m01 *= scaleVector.y;
	m11 *= scaleVector.y;
	m21 *= scaleVector.y;
	m02 *= scaleVector.z;
	m12 *= scaleVector.z;
	m22 *= scaleVector.z;
}

} // namespace aion::gameserver::geoEngine::math
