#pragma once

#include <cmath>

struct color_t
{
	unsigned char r;
	unsigned char g;
	unsigned char b;

	color_t() { }
	color_t(int r, int g, int b)
		: r((unsigned char)r), g((unsigned char)g), b((unsigned char)b) { }
};

struct image_t
{
	color_t *pixels;
	int resolution;

	image_t() :pixels(NULL), resolution(0) {}
	
	void init(int resolution)
	{
		this->resolution = resolution;
		pixels = new color_t[resolution * resolution];
	}

	void clear()
	{
		resolution = 0;
		delete pixels;
		pixels = NULL;
	}

	color_t get_pixel(int x, int y)
	{
		return pixels[y * resolution + x];
	}

	void set_pixel(int x, int y, color_t color)
	{
		pixels[y * resolution + x] = color;
	}
};

struct patch_t
{
	int x;
	int y;
	int size;
};

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
inline color_t get_color(vector3f_t v)
{
	return color_t(int(v.x * 255.0f), int(v.y * 255.0f), int(v.z * 255.0f));
}