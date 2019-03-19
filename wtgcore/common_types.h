#pragma once

#include <cmath>
#include <algorithm>

struct color_t
{
	unsigned char r;
	unsigned char g;
	unsigned char b;

	color_t() { }
	color_t(int r, int g, int b)
		: r((unsigned char)r), g((unsigned char)g), b((unsigned char)b) { }

	inline bool operator == (const color_t &other)
	{
		return r == other.r && g == other.g && b == other.b;
	}

	inline bool operator != (const color_t &other) { return !(*this == other); }
};

struct patch_t
{
	int x;
	int y;
	int size;
};

template <typename _p_t>
struct generic_image_t
{
	typedef _p_t _pixel_t;
	_pixel_t *pixels;
	int resolution;

	generic_image_t() :pixels(NULL), resolution(0) {}
	
	void init(int resolution)
	{
		this->resolution = resolution;
		pixels = new _pixel_t[resolution * resolution];
	}

	void clear()
	{
		resolution = 0;
		delete pixels;
		pixels = NULL;
	}

	_pixel_t get_pixel(int x, int y) const
	{
		return pixels[y * resolution + x];
	}

	void set_pixel(int x, int y, _pixel_t color)
	{
		pixels[y * resolution + x] = color;
	}

	_pixel_t get_pixel_in_patch(const patch_t &patch, int x, int y)
	{
		return get_pixel(patch.x + x, patch.y + y);
	}

	void set_pixel_in_patch(const patch_t &patch, int x, int y, _pixel_t color)
	{
		set_pixel(patch.x + x, patch.y + y, color);
	}

	_pixel_t get_pixel_wrapping(int x, int y) const
	{
		x = x % resolution;
		y = y % resolution;
		return get_pixel(x < 0 ? x + resolution : x, y < 0 ? y + resolution : y);
	}
};

typedef generic_image_t<color_t> image_t;
typedef generic_image_t<unsigned char> mask_t;

template <typename value_t>
struct vector3_t
{
	value_t x;
	value_t y;
	value_t z;
	
	vector3_t (value_t x, value_t y, value_t z)
		:x(x), y(y), z(z) { }

	vector3_t operator + (const vector3_t &other)
	{
		return vector3_t(x + other.x, y + other.y, z + other.z);
	}

	vector3_t operator - (const vector3_t &other)
	{
		return vector3_t(x - other.x, y - other.y, z - other.z);
	}

	vector3_t operator * (float k)
	{
		return vector3_t(x * k, y * k, z * k);
	}

	inline value_t sqrmagnitude() const
	{
		return x * x + y * y + z * z;
	}

	inline value_t magnitude() const
	{
		return sqrt(sqrmagnitude());
	}
};

typedef vector3_t<float> vector3f_t;

inline vector3f_t get_vector3f(color_t c)
{
	return vector3f_t(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f);
}

inline int _color_from_float(float x) { return std::max(std::min(int(x * 255.0f), 255), 0); }
inline color_t get_color(vector3f_t v)
{
	return color_t(_color_from_float(v.x), _color_from_float(v.y), _color_from_float(v.z));
}

#define CONSTRAINT_COLOR_SOURCE		color_t(255, 0, 0)
#define CONSTRAINT_COLOR_SINK		color_t(0, 255, 0)
#define CONSTRAINT_COLOR_FREE		color_t(100, 100, 100)