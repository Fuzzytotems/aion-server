#include "aion/gameserver/geoEngine/math/Matrix4f.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::math {

namespace {

const commons::logging::Logger& logger() {
	static const auto* l = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.math.Matrix4f"));
	return *l;
}

} // namespace

Matrix4f::Matrix4f(std::span<const float> array) {
	set(array, false);
}

void Matrix4f::copy(const Matrix4f& matrix) noexcept {
	m00 = matrix.m00;
	m01 = matrix.m01;
	m02 = matrix.m02;
	m03 = matrix.m03;
	m10 = matrix.m10;
	m11 = matrix.m11;
	m12 = matrix.m12;
	m13 = matrix.m13;
	m20 = matrix.m20;
	m21 = matrix.m21;
	m22 = matrix.m22;
	m23 = matrix.m23;
	m30 = matrix.m30;
	m31 = matrix.m31;
	m32 = matrix.m32;
	m33 = matrix.m33;
}

void Matrix4f::get(std::span<float> matrix, bool rowMajor) const {
	if (matrix.size() != 16)
		throw commons::utils::IllegalArgumentException("Array must be of size 16.");

	if (rowMajor) {
		matrix[0] = m00;
		matrix[1] = m01;
		matrix[2] = m02;
		matrix[3] = m03;
		matrix[4] = m10;
		matrix[5] = m11;
		matrix[6] = m12;
		matrix[7] = m13;
		matrix[8] = m20;
		matrix[9] = m21;
		matrix[10] = m22;
		matrix[11] = m23;
		matrix[12] = m30;
		matrix[13] = m31;
		matrix[14] = m32;
		matrix[15] = m33;
	} else {
		matrix[0] = m00;
		matrix[4] = m01;
		matrix[8] = m02;
		matrix[12] = m03;
		matrix[1] = m10;
		matrix[5] = m11;
		matrix[9] = m12;
		matrix[13] = m13;
		matrix[2] = m20;
		matrix[6] = m21;
		matrix[10] = m22;
		matrix[14] = m23;
		matrix[3] = m30;
		matrix[7] = m31;
		matrix[11] = m32;
		matrix[15] = m33;
	}
}

float Matrix4f::get(int32_t i, int32_t j) const {
	switch (i) {
		case 0:
			switch (j) {
				case 0:
					return m00;
				case 1:
					return m01;
				case 2:
					return m02;
				case 3:
					return m03;
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
				case 3:
					return m13;
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
				case 3:
					return m23;
			}
			break;
		case 3:
			switch (j) {
				case 0:
					return m30;
				case 1:
					return m31;
				case 2:
					return m32;
				case 3:
					return m33;
			}
	}
	logger().warn("Invalid matrix index.");
	throw commons::utils::IllegalArgumentException("Invalid indices into matrix.");
}

std::array<float, 4> Matrix4f::getColumn(int32_t i) const {
	std::array<float, 4> store{};
	getColumn(i, store);
	return store;
}

std::span<float, 4> Matrix4f::getColumn(int32_t i, std::span<float, 4> store) const {
	switch (i) {
		case 0:
			store[0] = m00;
			store[1] = m10;
			store[2] = m20;
			store[3] = m30;
			break;
		case 1:
			store[0] = m01;
			store[1] = m11;
			store[2] = m21;
			store[3] = m31;
			break;
		case 2:
			store[0] = m02;
			store[1] = m12;
			store[2] = m22;
			store[3] = m32;
			break;
		case 3:
			store[0] = m03;
			store[1] = m13;
			store[2] = m23;
			store[3] = m33;
			break;
		default:
			logger().warn("Invalid column index.");
			throw commons::utils::IllegalArgumentException("Invalid column index. " + std::to_string(i));
	}
	return store;
}

void Matrix4f::setColumn(int32_t i, std::span<const float, 4> column) {
	switch (i) {
		case 0:
			m00 = column[0];
			m10 = column[1];
			m20 = column[2];
			m30 = column[3];
			break;
		case 1:
			m01 = column[0];
			m11 = column[1];
			m21 = column[2];
			m31 = column[3];
			break;
		case 2:
			m02 = column[0];
			m12 = column[1];
			m22 = column[2];
			m32 = column[3];
			break;
		case 3:
			m03 = column[0];
			m13 = column[1];
			m23 = column[2];
			m33 = column[3];
			break;
		default:
			logger().warn("Invalid column index.");
			throw commons::utils::IllegalArgumentException("Invalid column index. " + std::to_string(i));
	}
}

void Matrix4f::set(int32_t i, int32_t j, float value) {
	switch (i) {
		case 0:
			switch (j) {
				case 0:
					m00 = value;
					return;
				case 1:
					m01 = value;
					return;
				case 2:
					m02 = value;
					return;
				case 3:
					m03 = value;
					return;
			}
			break;
		case 1:
			switch (j) {
				case 0:
					m10 = value;
					return;
				case 1:
					m11 = value;
					return;
				case 2:
					m12 = value;
					return;
				case 3:
					m13 = value;
					return;
			}
			break;
		case 2:
			switch (j) {
				case 0:
					m20 = value;
					return;
				case 1:
					m21 = value;
					return;
				case 2:
					m22 = value;
					return;
				case 3:
					m23 = value;
					return;
			}
			break;
		case 3:
			switch (j) {
				case 0:
					m30 = value;
					return;
				case 1:
					m31 = value;
					return;
				case 2:
					m32 = value;
					return;
				case 3:
					m33 = value;
					return;
			}
	}
	logger().warn("Invalid matrix index.");
	throw commons::utils::IllegalArgumentException("Invalid indices into matrix.");
}

Matrix4f& Matrix4f::set(const Matrix4f& matrix) noexcept {
	m00 = matrix.m00;
	m01 = matrix.m01;
	m02 = matrix.m02;
	m03 = matrix.m03;
	m10 = matrix.m10;
	m11 = matrix.m11;
	m12 = matrix.m12;
	m13 = matrix.m13;
	m20 = matrix.m20;
	m21 = matrix.m21;
	m22 = matrix.m22;
	m23 = matrix.m23;
	m30 = matrix.m30;
	m31 = matrix.m31;
	m32 = matrix.m32;
	m33 = matrix.m33;
	return *this;
}

void Matrix4f::set(std::span<const float> matrix, bool rowMajor) {
	if (matrix.size() != 16)
		throw commons::utils::IllegalArgumentException("Array must be of size 16.");

	if (rowMajor) {
		m00 = matrix[0];
		m01 = matrix[1];
		m02 = matrix[2];
		m03 = matrix[3];
		m10 = matrix[4];
		m11 = matrix[5];
		m12 = matrix[6];
		m13 = matrix[7];
		m20 = matrix[8];
		m21 = matrix[9];
		m22 = matrix[10];
		m23 = matrix[11];
		m30 = matrix[12];
		m31 = matrix[13];
		m32 = matrix[14];
		m33 = matrix[15];
	} else {
		m00 = matrix[0];
		m01 = matrix[4];
		m02 = matrix[8];
		m03 = matrix[12];
		m10 = matrix[1];
		m11 = matrix[5];
		m12 = matrix[9];
		m13 = matrix[13];
		m20 = matrix[2];
		m21 = matrix[6];
		m22 = matrix[10];
		m23 = matrix[14];
		m30 = matrix[3];
		m31 = matrix[7];
		m32 = matrix[11];
		m33 = matrix[15];
	}
}

Matrix4f Matrix4f::transpose() const noexcept {
	std::array<float, 16> tmp{};
	get(tmp, true);
	return Matrix4f(tmp);
}

Matrix4f& Matrix4f::transposeLocal() noexcept {
	float tmp = m01;
	m01 = m10;
	m10 = tmp;

	tmp = m02;
	m02 = m20;
	m20 = tmp;

	tmp = m03;
	m03 = m30;
	m30 = tmp;

	tmp = m12;
	m12 = m21;
	m21 = tmp;

	tmp = m13;
	m13 = m31;
	m31 = tmp;

	tmp = m23;
	m23 = m32;
	m32 = tmp;

	return *this;
}

void Matrix4f::fillFloatArray(std::span<float, 16> f, bool columnMajor) const noexcept {
	if (columnMajor) {
		f[0] = m00;
		f[1] = m10;
		f[2] = m20;
		f[3] = m30;
		f[4] = m01;
		f[5] = m11;
		f[6] = m21;
		f[7] = m31;
		f[8] = m02;
		f[9] = m12;
		f[10] = m22;
		f[11] = m32;
		f[12] = m03;
		f[13] = m13;
		f[14] = m23;
		f[15] = m33;
	} else {
		f[0] = m00;
		f[1] = m01;
		f[2] = m02;
		f[3] = m03;
		f[4] = m10;
		f[5] = m11;
		f[6] = m12;
		f[7] = m13;
		f[8] = m20;
		f[9] = m21;
		f[10] = m22;
		f[11] = m23;
		f[12] = m30;
		f[13] = m31;
		f[14] = m32;
		f[15] = m33;
	}
}

void Matrix4f::loadIdentity() noexcept {
	m01 = m02 = m03 = 0.0f;
	m10 = m12 = m13 = 0.0f;
	m20 = m21 = m23 = 0.0f;
	m30 = m31 = m32 = 0.0f;
	m00 = m11 = m22 = m33 = 1.0f;
}

void Matrix4f::fromAngleAxis(float angle, const Vector3f& axis) noexcept {
	const Vector3f normAxis = axis.normalize();
	fromAngleNormalAxis(angle, normAxis);
}

void Matrix4f::fromAngleNormalAxis(float angle, const Vector3f& axis) noexcept {
	zero();
	m33 = 1;

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

void Matrix4f::multLocal(float scalar) noexcept {
	m00 *= scalar;
	m01 *= scalar;
	m02 *= scalar;
	m03 *= scalar;
	m10 *= scalar;
	m11 *= scalar;
	m12 *= scalar;
	m13 *= scalar;
	m20 *= scalar;
	m21 *= scalar;
	m22 *= scalar;
	m23 *= scalar;
	m30 *= scalar;
	m31 *= scalar;
	m32 *= scalar;
	m33 *= scalar;
}

Matrix4f Matrix4f::mult(float scalar) const noexcept {
	Matrix4f out;
	out.set(*this);
	out.multLocal(scalar);
	return out;
}

Matrix4f& Matrix4f::mult(float scalar, Matrix4f& store) const noexcept {
	store.set(*this);
	store.multLocal(scalar);
	return store;
}

Matrix4f Matrix4f::mult(const Matrix4f& in2) const noexcept {
	Matrix4f store;
	mult(in2, store);
	return store;
}

Matrix4f& Matrix4f::mult(const Matrix4f& in2, Matrix4f& store) const noexcept {
	float temp00, temp01, temp02, temp03;
	float temp10, temp11, temp12, temp13;
	float temp20, temp21, temp22, temp23;
	float temp30, temp31, temp32, temp33;

	temp00 = m00 * in2.m00 + m01 * in2.m10 + m02 * in2.m20 + m03 * in2.m30;
	temp01 = m00 * in2.m01 + m01 * in2.m11 + m02 * in2.m21 + m03 * in2.m31;
	temp02 = m00 * in2.m02 + m01 * in2.m12 + m02 * in2.m22 + m03 * in2.m32;
	temp03 = m00 * in2.m03 + m01 * in2.m13 + m02 * in2.m23 + m03 * in2.m33;

	temp10 = m10 * in2.m00 + m11 * in2.m10 + m12 * in2.m20 + m13 * in2.m30;
	temp11 = m10 * in2.m01 + m11 * in2.m11 + m12 * in2.m21 + m13 * in2.m31;
	temp12 = m10 * in2.m02 + m11 * in2.m12 + m12 * in2.m22 + m13 * in2.m32;
	temp13 = m10 * in2.m03 + m11 * in2.m13 + m12 * in2.m23 + m13 * in2.m33;

	temp20 = m20 * in2.m00 + m21 * in2.m10 + m22 * in2.m20 + m23 * in2.m30;
	temp21 = m20 * in2.m01 + m21 * in2.m11 + m22 * in2.m21 + m23 * in2.m31;
	temp22 = m20 * in2.m02 + m21 * in2.m12 + m22 * in2.m22 + m23 * in2.m32;
	temp23 = m20 * in2.m03 + m21 * in2.m13 + m22 * in2.m23 + m23 * in2.m33;

	temp30 = m30 * in2.m00 + m31 * in2.m10 + m32 * in2.m20 + m33 * in2.m30;
	temp31 = m30 * in2.m01 + m31 * in2.m11 + m32 * in2.m21 + m33 * in2.m31;
	temp32 = m30 * in2.m02 + m31 * in2.m12 + m32 * in2.m22 + m33 * in2.m32;
	temp33 = m30 * in2.m03 + m31 * in2.m13 + m32 * in2.m23 + m33 * in2.m33;

	store.m00 = temp00;
	store.m01 = temp01;
	store.m02 = temp02;
	store.m03 = temp03;
	store.m10 = temp10;
	store.m11 = temp11;
	store.m12 = temp12;
	store.m13 = temp13;
	store.m20 = temp20;
	store.m21 = temp21;
	store.m22 = temp22;
	store.m23 = temp23;
	store.m30 = temp30;
	store.m31 = temp31;
	store.m32 = temp32;
	store.m33 = temp33;

	return store;
}

Matrix4f& Matrix4f::multLocal(const Matrix4f& in2) noexcept {
	return mult(in2, *this);
}

Vector3f Matrix4f::mult(const Vector3f& vec) const noexcept {
	Vector3f store;
	mult(vec, store);
	return store;
}

Vector3f& Matrix4f::mult(const Vector3f& vec, Vector3f& store) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;
	store.x = m00 * vx + m01 * vy + m02 * vz + m03;
	store.y = m10 * vx + m11 * vy + m12 * vz + m13;
	store.z = m20 * vx + m21 * vy + m22 * vz + m23;
	return store;
}

Vector3f& Matrix4f::multNormal(const Vector3f& vec, Vector3f& store) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;
	store.x = m00 * vx + m01 * vy + m02 * vz;
	store.y = m10 * vx + m11 * vy + m12 * vz;
	store.z = m20 * vx + m21 * vy + m22 * vz;
	return store;
}

Vector3f& Matrix4f::multNormalAcross(const Vector3f& vec, Vector3f& store) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;
	store.x = m00 * vx + m10 * vy + m20 * vz;
	store.y = m01 * vx + m11 * vy + m21 * vz;
	store.z = m02 * vx + m12 * vy + m22 * vz;
	return store;
}

float Matrix4f::multProj(const Vector3f& vec, Vector3f& store) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;
	store.x = m00 * vx + m01 * vy + m02 * vz + m03;
	store.y = m10 * vx + m11 * vy + m12 * vz + m13;
	store.z = m20 * vx + m21 * vy + m22 * vz + m23;
	return m30 * vx + m31 * vy + m32 * vz + m33;
}

Vector3f& Matrix4f::multAcross(const Vector3f& vec, Vector3f& store) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;
	store.x = m00 * vx + m10 * vy + m20 * vz + m30 * 1;
	store.y = m01 * vx + m11 * vy + m21 * vz + m31 * 1;
	store.z = m02 * vx + m12 * vy + m22 * vz + m32 * 1;
	return store;
}

std::span<float, 4> Matrix4f::mult(std::span<float, 4> vec4f) const noexcept {
	const float x = vec4f[0], y = vec4f[1], z = vec4f[2], w = vec4f[3];

	vec4f[0] = m00 * x + m01 * y + m02 * z + m03 * w;
	vec4f[1] = m10 * x + m11 * y + m12 * z + m13 * w;
	vec4f[2] = m20 * x + m21 * y + m22 * z + m23 * w;
	vec4f[3] = m30 * x + m31 * y + m32 * z + m33 * w;

	return vec4f;
}

std::span<float, 4> Matrix4f::multAcross(std::span<float, 4> vec4f) const noexcept {
	const float x = vec4f[0], y = vec4f[1], z = vec4f[2], w = vec4f[3];

	vec4f[0] = m00 * x + m10 * y + m20 * z + m30 * w;
	vec4f[1] = m01 * x + m11 * y + m21 * z + m31 * w;
	vec4f[2] = m02 * x + m12 * y + m22 * z + m32 * w;
	vec4f[3] = m03 * x + m13 * y + m23 * z + m33 * w;

	return vec4f;
}

Matrix4f Matrix4f::invert() const {
	Matrix4f store;
	invert(store);
	return store;
}

Matrix4f& Matrix4f::invert(Matrix4f& store) const {
	const float fA0 = m00 * m11 - m01 * m10;
	const float fA1 = m00 * m12 - m02 * m10;
	const float fA2 = m00 * m13 - m03 * m10;
	const float fA3 = m01 * m12 - m02 * m11;
	const float fA4 = m01 * m13 - m03 * m11;
	const float fA5 = m02 * m13 - m03 * m12;
	const float fB0 = m20 * m31 - m21 * m30;
	const float fB1 = m20 * m32 - m22 * m30;
	const float fB2 = m20 * m33 - m23 * m30;
	const float fB3 = m21 * m32 - m22 * m31;
	const float fB4 = m21 * m33 - m23 * m31;
	const float fB5 = m22 * m33 - m23 * m32;
	const float fDet = fA0 * fB5 - fA1 * fB4 + fA2 * fB3 + fA3 * fB2 - fA4 * fB1 + fA5 * fB0;

	if (FastMath::abs(fDet) <= 0.0f)
		throw ArithmeticException("This matrix cannot be inverted");

	store.m00 = +m11 * fB5 - m12 * fB4 + m13 * fB3;
	store.m10 = -m10 * fB5 + m12 * fB2 - m13 * fB1;
	store.m20 = +m10 * fB4 - m11 * fB2 + m13 * fB0;
	store.m30 = -m10 * fB3 + m11 * fB1 - m12 * fB0;
	store.m01 = -m01 * fB5 + m02 * fB4 - m03 * fB3;
	store.m11 = +m00 * fB5 - m02 * fB2 + m03 * fB1;
	store.m21 = -m00 * fB4 + m01 * fB2 - m03 * fB0;
	store.m31 = +m00 * fB3 - m01 * fB1 + m02 * fB0;
	store.m02 = +m31 * fA5 - m32 * fA4 + m33 * fA3;
	store.m12 = -m30 * fA5 + m32 * fA2 - m33 * fA1;
	store.m22 = +m30 * fA4 - m31 * fA2 + m33 * fA0;
	store.m32 = -m30 * fA3 + m31 * fA1 - m32 * fA0;
	store.m03 = -m21 * fA5 + m22 * fA4 - m23 * fA3;
	store.m13 = +m20 * fA5 - m22 * fA2 + m23 * fA1;
	store.m23 = -m20 * fA4 + m21 * fA2 - m23 * fA0;
	store.m33 = +m20 * fA3 - m21 * fA1 + m22 * fA0;

	const float fInvDet = 1.0f / fDet;
	store.multLocal(fInvDet);

	return store;
}

Matrix4f& Matrix4f::invertLocal() noexcept {
	const float fA0 = m00 * m11 - m01 * m10;
	const float fA1 = m00 * m12 - m02 * m10;
	const float fA2 = m00 * m13 - m03 * m10;
	const float fA3 = m01 * m12 - m02 * m11;
	const float fA4 = m01 * m13 - m03 * m11;
	const float fA5 = m02 * m13 - m03 * m12;
	const float fB0 = m20 * m31 - m21 * m30;
	const float fB1 = m20 * m32 - m22 * m30;
	const float fB2 = m20 * m33 - m23 * m30;
	const float fB3 = m21 * m32 - m22 * m31;
	const float fB4 = m21 * m33 - m23 * m31;
	const float fB5 = m22 * m33 - m23 * m32;
	const float fDet = fA0 * fB5 - fA1 * fB4 + fA2 * fB3 + fA3 * fB2 - fA4 * fB1 + fA5 * fB0;

	if (FastMath::abs(fDet) <= 0.0f)
		return zero();

	const float f00 = +m11 * fB5 - m12 * fB4 + m13 * fB3;
	const float f10 = -m10 * fB5 + m12 * fB2 - m13 * fB1;
	const float f20 = +m10 * fB4 - m11 * fB2 + m13 * fB0;
	const float f30 = -m10 * fB3 + m11 * fB1 - m12 * fB0;
	const float f01 = -m01 * fB5 + m02 * fB4 - m03 * fB3;
	const float f11 = +m00 * fB5 - m02 * fB2 + m03 * fB1;
	const float f21 = -m00 * fB4 + m01 * fB2 - m03 * fB0;
	const float f31 = +m00 * fB3 - m01 * fB1 + m02 * fB0;
	const float f02 = +m31 * fA5 - m32 * fA4 + m33 * fA3;
	const float f12 = -m30 * fA5 + m32 * fA2 - m33 * fA1;
	const float f22 = +m30 * fA4 - m31 * fA2 + m33 * fA0;
	const float f32 = -m30 * fA3 + m31 * fA1 - m32 * fA0;
	const float f03 = -m21 * fA5 + m22 * fA4 - m23 * fA3;
	const float f13 = +m20 * fA5 - m22 * fA2 + m23 * fA1;
	const float f23 = -m20 * fA4 + m21 * fA2 - m23 * fA0;
	const float f33 = +m20 * fA3 - m21 * fA1 + m22 * fA0;

	m00 = f00;
	m01 = f01;
	m02 = f02;
	m03 = f03;
	m10 = f10;
	m11 = f11;
	m12 = f12;
	m13 = f13;
	m20 = f20;
	m21 = f21;
	m22 = f22;
	m23 = f23;
	m30 = f30;
	m31 = f31;
	m32 = f32;
	m33 = f33;

	const float fInvDet = 1.0f / fDet;
	multLocal(fInvDet);

	return *this;
}

Matrix4f Matrix4f::adjoint() const noexcept {
	Matrix4f store;
	adjoint(store);
	return store;
}

Matrix4f& Matrix4f::adjoint(Matrix4f& store) const noexcept {
	const float fA0 = m00 * m11 - m01 * m10;
	const float fA1 = m00 * m12 - m02 * m10;
	const float fA2 = m00 * m13 - m03 * m10;
	const float fA3 = m01 * m12 - m02 * m11;
	const float fA4 = m01 * m13 - m03 * m11;
	const float fA5 = m02 * m13 - m03 * m12;
	const float fB0 = m20 * m31 - m21 * m30;
	const float fB1 = m20 * m32 - m22 * m30;
	const float fB2 = m20 * m33 - m23 * m30;
	const float fB3 = m21 * m32 - m22 * m31;
	const float fB4 = m21 * m33 - m23 * m31;
	const float fB5 = m22 * m33 - m23 * m32;

	store.m00 = +m11 * fB5 - m12 * fB4 + m13 * fB3;
	store.m10 = -m10 * fB5 + m12 * fB2 - m13 * fB1;
	store.m20 = +m10 * fB4 - m11 * fB2 + m13 * fB0;
	store.m30 = -m10 * fB3 + m11 * fB1 - m12 * fB0;
	store.m01 = -m01 * fB5 + m02 * fB4 - m03 * fB3;
	store.m11 = +m00 * fB5 - m02 * fB2 + m03 * fB1;
	store.m21 = -m00 * fB4 + m01 * fB2 - m03 * fB0;
	store.m31 = +m00 * fB3 - m01 * fB1 + m02 * fB0;
	store.m02 = +m31 * fA5 - m32 * fA4 + m33 * fA3;
	store.m12 = -m30 * fA5 + m32 * fA2 - m33 * fA1;
	store.m22 = +m30 * fA4 - m31 * fA2 + m33 * fA0;
	store.m32 = -m30 * fA3 + m31 * fA1 - m32 * fA0;
	store.m03 = -m21 * fA5 + m22 * fA4 - m23 * fA3;
	store.m13 = +m20 * fA5 - m22 * fA2 + m23 * fA1;
	store.m23 = -m20 * fA4 + m21 * fA2 - m23 * fA0;
	store.m33 = +m20 * fA3 - m21 * fA1 + m22 * fA0;

	return store;
}

float Matrix4f::determinant() const noexcept {
	const float fA0 = m00 * m11 - m01 * m10;
	const float fA1 = m00 * m12 - m02 * m10;
	const float fA2 = m00 * m13 - m03 * m10;
	const float fA3 = m01 * m12 - m02 * m11;
	const float fA4 = m01 * m13 - m03 * m11;
	const float fA5 = m02 * m13 - m03 * m12;
	const float fB0 = m20 * m31 - m21 * m30;
	const float fB1 = m20 * m32 - m22 * m30;
	const float fB2 = m20 * m33 - m23 * m30;
	const float fB3 = m21 * m32 - m22 * m31;
	const float fB4 = m21 * m33 - m23 * m31;
	const float fB5 = m22 * m33 - m23 * m32;
	const float fDet = fA0 * fB5 - fA1 * fB4 + fA2 * fB3 + fA3 * fB2 - fA4 * fB1 + fA5 * fB0;
	return fDet;
}

Matrix4f& Matrix4f::zero() noexcept {
	m00 = m01 = m02 = m03 = 0.0f;
	m10 = m11 = m12 = m13 = 0.0f;
	m20 = m21 = m22 = m23 = 0.0f;
	m30 = m31 = m32 = m33 = 0.0f;
	return *this;
}

Matrix4f Matrix4f::add(const Matrix4f& mat) const noexcept {
	Matrix4f result;
	result.m00 = this->m00 + mat.m00;
	result.m01 = this->m01 + mat.m01;
	result.m02 = this->m02 + mat.m02;
	result.m03 = this->m03 + mat.m03;
	result.m10 = this->m10 + mat.m10;
	result.m11 = this->m11 + mat.m11;
	result.m12 = this->m12 + mat.m12;
	result.m13 = this->m13 + mat.m13;
	result.m20 = this->m20 + mat.m20;
	result.m21 = this->m21 + mat.m21;
	result.m22 = this->m22 + mat.m22;
	result.m23 = this->m23 + mat.m23;
	result.m30 = this->m30 + mat.m30;
	result.m31 = this->m31 + mat.m31;
	result.m32 = this->m32 + mat.m32;
	result.m33 = this->m33 + mat.m33;
	return result;
}

void Matrix4f::addLocal(const Matrix4f& mat) noexcept {
	m00 += mat.m00;
	m01 += mat.m01;
	m02 += mat.m02;
	m03 += mat.m03;
	m10 += mat.m10;
	m11 += mat.m11;
	m12 += mat.m12;
	m13 += mat.m13;
	m20 += mat.m20;
	m21 += mat.m21;
	m22 += mat.m22;
	m23 += mat.m23;
	m30 += mat.m30;
	m31 += mat.m31;
	m32 += mat.m32;
	m33 += mat.m33;
}

Vector3f Matrix4f::toTranslationVector() const noexcept {
	return Vector3f(m03, m13, m23);
}

void Matrix4f::toTranslationVector(Vector3f& vector) const noexcept {
	vector.set(m03, m13, m23);
}

Matrix3f Matrix4f::toRotationMatrix() const noexcept {
	return Matrix3f(m00, m01, m02, m10, m11, m12, m20, m21, m22);
}

void Matrix4f::toRotationMatrix(Matrix3f& mat) const noexcept {
	mat.m00 = m00;
	mat.m01 = m01;
	mat.m02 = m02;
	mat.m10 = m10;
	mat.m11 = m11;
	mat.m12 = m12;
	mat.m20 = m20;
	mat.m21 = m21;
	mat.m22 = m22;
}

void Matrix4f::setRotationMatrix(const Matrix3f& mat) noexcept {
	this->m00 = mat.m00;
	this->m01 = mat.m01;
	this->m02 = mat.m02;
	this->m10 = mat.m10;
	this->m11 = mat.m11;
	this->m12 = mat.m12;
	this->m20 = mat.m20;
	this->m21 = mat.m21;
	this->m22 = mat.m22;
}

void Matrix4f::setScale(float x, float y, float z) noexcept {
	m00 *= x;
	m11 *= y;
	m22 *= z;
}

void Matrix4f::setScale(const Vector3f& scaleVector) noexcept {
	m00 *= scaleVector.x;
	m11 *= scaleVector.y;
	m22 *= scaleVector.z;
}

void Matrix4f::setTranslation(std::span<const float> translation) {
	if (translation.size() != 3)
		throw commons::utils::IllegalArgumentException("Translation size must be 3.");
	m03 = translation[0];
	m13 = translation[1];
	m23 = translation[2];
}

void Matrix4f::setTranslation(float x, float y, float z) noexcept {
	m03 = x;
	m13 = y;
	m23 = z;
}

void Matrix4f::setTranslation(const Vector3f& translation) noexcept {
	m03 = translation.x;
	m13 = translation.y;
	m23 = translation.z;
}

void Matrix4f::setInverseTranslation(std::span<const float> translation) {
	if (translation.size() != 3)
		throw commons::utils::IllegalArgumentException("Translation size must be 3.");
	m03 = -translation[0];
	m13 = -translation[1];
	m23 = -translation[2];
}

void Matrix4f::angleRotation(const Vector3f& angles) noexcept {
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = (angles.z * FastMath::DEG_TO_RAD);
	sy = FastMath::sin(angle);
	cy = FastMath::cos(angle);
	angle = (angles.y * FastMath::DEG_TO_RAD);
	sp = FastMath::sin(angle);
	cp = FastMath::cos(angle);
	angle = (angles.x * FastMath::DEG_TO_RAD);
	sr = FastMath::sin(angle);
	cr = FastMath::cos(angle);

	// matrix = (Z * Y) * X
	m00 = cp * cy;
	m10 = cp * sy;
	m20 = -sp;
	m01 = sr * sp * cy + cr * -sy;
	m11 = sr * sp * sy + cr * cy;
	m21 = sr * cp;
	m02 = (cr * sp * cy + -sr * -sy);
	m12 = (cr * sp * sy + -sr * cy);
	m22 = cr * cp;
	m03 = 0.0f;
	m13 = 0.0f;
	m23 = 0.0f;
}

void Matrix4f::setInverseRotationRadians(std::span<const float> angles) {
	if (angles.size() != 3)
		throw commons::utils::IllegalArgumentException("Angles must be of size 3.");
	const double cr = FastMath::cos(angles[0]);
	const double sr = FastMath::sin(angles[0]);
	const double cp = FastMath::cos(angles[1]);
	const double sp = FastMath::sin(angles[1]);
	const double cy = FastMath::cos(angles[2]);
	const double sy = FastMath::sin(angles[2]);

	m00 = static_cast<float>(cp * cy);
	m10 = static_cast<float>(cp * sy);
	m20 = static_cast<float>(-sp);

	const double srsp = sr * sp;
	const double crsp = cr * sp;

	m01 = static_cast<float>(srsp * cy - cr * sy);
	m11 = static_cast<float>(srsp * sy + cr * cy);
	m21 = static_cast<float>(sr * cp);

	m02 = static_cast<float>(crsp * cy + sr * sy);
	m12 = static_cast<float>(crsp * sy - sr * cy);
	m22 = static_cast<float>(cr * cp);
}

void Matrix4f::setInverseRotationDegrees(std::span<const float> angles) {
	if (angles.size() != 3)
		throw commons::utils::IllegalArgumentException("Angles must be of size 3.");
	const std::array<float, 3> vec = {angles[0] * FastMath::RAD_TO_DEG, angles[1] * FastMath::RAD_TO_DEG, angles[2] * FastMath::RAD_TO_DEG};
	setInverseRotationRadians(vec);
}

void Matrix4f::inverseTranslateVect(std::span<float> vec) const {
	if (vec.size() != 3)
		throw commons::utils::IllegalArgumentException("vec must be of size 3.");

	vec[0] = vec[0] - m03;
	vec[1] = vec[1] - m13;
	vec[2] = vec[2] - m23;
}

void Matrix4f::inverseTranslateVect(Vector3f& data) const noexcept {
	data.x -= m03;
	data.y -= m13;
	data.z -= m23;
}

void Matrix4f::translateVect(Vector3f& data) const noexcept {
	data.x += m03;
	data.y += m13;
	data.z += m23;
}

void Matrix4f::inverseRotateVect(Vector3f& vec) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;

	vec.x = vx * m00 + vy * m10 + vz * m20;
	vec.y = vx * m01 + vy * m11 + vz * m21;
	vec.z = vx * m02 + vy * m12 + vz * m22;
}

void Matrix4f::rotateVect(Vector3f& vec) const noexcept {
	const float vx = vec.x, vy = vec.y, vz = vec.z;

	vec.x = vx * m00 + vy * m01 + vz * m02;
	vec.y = vx * m10 + vy * m11 + vz * m12;
	vec.z = vx * m20 + vy * m21 + vz * m22;
}

std::string Matrix4f::toString() const {
	std::string result = "Matrix4f\n[\n";
	result +=
	  " " + JavaFloat::toString(m00) + "  " + JavaFloat::toString(m01) + "  " + JavaFloat::toString(m02) + "  " + JavaFloat::toString(m03) + " \n";
	result +=
	  " " + JavaFloat::toString(m10) + "  " + JavaFloat::toString(m11) + "  " + JavaFloat::toString(m12) + "  " + JavaFloat::toString(m13) + " \n";
	result +=
	  " " + JavaFloat::toString(m20) + "  " + JavaFloat::toString(m21) + "  " + JavaFloat::toString(m22) + "  " + JavaFloat::toString(m23) + " \n";
	result +=
	  " " + JavaFloat::toString(m30) + "  " + JavaFloat::toString(m31) + "  " + JavaFloat::toString(m32) + "  " + JavaFloat::toString(m33) + " \n]";
	return result;
}

int32_t Matrix4f::hashCode() const noexcept {
	// Java int arithmetic wraps: computed in uint32_t
	uint32_t hash = 37;
	for (float value : {m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33})
		hash = 37 * hash + static_cast<uint32_t>(JavaFloat::floatToIntBits(value));
	return static_cast<int32_t>(hash);
}

bool Matrix4f::equals(const Matrix4f& o) const noexcept {
	if (this == &o)
		return true;
	const std::array<float, 16> a = {m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33};
	const std::array<float, 16> b = {o.m00, o.m01, o.m02, o.m03, o.m10, o.m11, o.m12, o.m13, o.m20, o.m21, o.m22, o.m23, o.m30, o.m31, o.m32, o.m33};
	for (size_t i = 0; i < a.size(); i++) {
		if (JavaFloat::compare(a[i], b[i]) != 0)
			return false;
	}
	return true;
}

bool Matrix4f::isIdentity() const noexcept {
	return (m00 == 1 && m01 == 0 && m02 == 0 && m03 == 0) && (m10 == 0 && m11 == 1 && m12 == 0 && m13 == 0) &&
	       (m20 == 0 && m21 == 0 && m22 == 1 && m23 == 0) && (m30 == 0 && m31 == 0 && m32 == 0 && m33 == 1);
}

void Matrix4f::scale(const Vector3f& scaleVector) noexcept {
	m00 *= scaleVector.getX();
	m10 *= scaleVector.getX();
	m20 *= scaleVector.getX();
	m30 *= scaleVector.getX();
	m01 *= scaleVector.getY();
	m11 *= scaleVector.getY();
	m21 *= scaleVector.getY();
	m31 *= scaleVector.getY();
	m02 *= scaleVector.getZ();
	m12 *= scaleVector.getZ();
	m22 *= scaleVector.getZ();
	m32 *= scaleVector.getZ();
}

void Matrix4f::scale(float factor) noexcept {
	m00 *= factor;
	m10 *= factor;
	m20 *= factor;
	m30 *= factor;
	m01 *= factor;
	m11 *= factor;
	m21 *= factor;
	m31 *= factor;
	m02 *= factor;
	m12 *= factor;
	m22 *= factor;
	m32 *= factor;
}

} // namespace aion::gameserver::geoEngine::math
