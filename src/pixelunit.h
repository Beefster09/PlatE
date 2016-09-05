#pragma once

/** Pixel-perfect coordinate component
* It's a class purely for implicit conversions and += operators
*/
class PixelUnit {
public:
	int32_t pixel;
private:
	float subpixel; // must be private to keep within [0, 1)
	inline void normalize() { // Essentially just a hygenic macro
		float extra = floor(subpixel);
		pixel += extra;
		subpixel -= extra;
		assert(subpixel >= 0.0 && subpixel < 1.0);
	}

public:
	inline PixelUnit() { pixel = 0; subpixel = 0.0f; }
	inline PixelUnit(const PixelUnit& copy) { pixel = copy.pixel; subpixel = copy.subpixel; }

	// Implicit conversions
	inline PixelUnit(double d) { pixel = (int)floor(d); subpixel = d - pixel; }
	inline PixelUnit(float f) { pixel = (int)floor(f); subpixel = f - pixel; }
	inline PixelUnit(int pix) { pixel = pix; subpixel = 0.0f; }

	inline operator int() const { return pixel; }
	inline operator double() const { return (double)subpixel + pixel; }
	inline operator float() const { return subpixel + pixel; }

	// Operators
	inline void operator += (const PixelUnit& other) {
		pixel += other.pixel;
		subpixel += other.subpixel;
		normalize();
	}
	inline void operator -= (const PixelUnit& other) {
		pixel -= other.pixel;
		subpixel -= other.subpixel;
		normalize();
	}

	inline void operator += (int other) { pixel += other; }
	inline void operator -= (int other) { pixel -= other; }
	inline void operator += (float other) { subpixel += other; normalize(); }
	inline void operator -= (float other) { subpixel -= other; normalize(); }

	inline friend PixelUnit& operator + (const PixelUnit& lhs, const PixelUnit& rhs) {
		PixelUnit result = lhs;
		result += rhs;
		return result;
	}

	inline friend PixelUnit& operator - (const PixelUnit& lhs, const PixelUnit& rhs) {
		PixelUnit result = lhs;
		result -= rhs;
		return result;
	}

	// Note that there are no * or / operators. This is not a mistake.
	// PixelUnits should only ever be translated.
};