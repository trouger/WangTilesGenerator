// wtgcore.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include "common_types.h"
#include "wangtiles.h"
 

color_t *readfile(const char *path, int resolution)
{
	FILE *f;
	if (fopen_s(&f, path, "rb")) return NULL;
	size_t pixel_count = resolution * resolution;
	color_t *data = new color_t[pixel_count];
	// python image is in reversed row order (top row first)
	color_t *pbuffer = data + pixel_count - resolution;
	for (int i = 0; i < resolution; i++)
	{
		size_t r = fread((void *)pbuffer, sizeof(color_t), resolution, f);
		if (r != resolution)
		{
			fclose(f);
			delete data;
			return NULL;
		}
		pbuffer -= resolution;
	}
	fclose(f);
	return data;
}

bool writefile(const char *path, const color_t *data, int resolution)
{
	FILE *f;
	if (fopen_s(&f, path, "wb")) return false;
	size_t pixel_count = resolution * resolution;
	// python image is in reversed row order (top row first)
	const color_t *pbuffer = data + pixel_count - resolution;
	for (int i = 0; i < resolution; i++)
	{
		size_t r = fwrite((const void *)pbuffer, sizeof(color_t), resolution, f);
		if (r != resolution)
		{
			fclose(f);
			return false;
		}
		pbuffer -= resolution;
	}
	fclose(f);
	return true;
}

bool writefile(const char *path, const color_t *data, const unsigned char *alpha, int resolution)
{
	FILE *f;
	if (fopen_s(&f, path, "wb")) return false;
	size_t pixel_count = resolution * resolution;
	// python image is in reversed row order (top row first)
	const color_t *pbuffer = data + pixel_count - resolution;
	const unsigned char *palpha = alpha + pixel_count - resolution;
	for (int i = 0; i < resolution; i++)
	{
		size_t r = 0;
		for (int j = 0; j < resolution; j++)
		{
			r += fwrite((const void *)(pbuffer + j), sizeof(color_t), 1, f);
			r += fwrite((const void *)(palpha + j), sizeof(unsigned char), 1, f);
		}
		if (r != resolution * 2)
		{
			fclose(f);
			return false;
		}
		pbuffer -= resolution;
		palpha -= resolution;
	}
	fclose(f);
	return true;
}

struct resultset_t
{
	image_t packed_cornors;
	mask_t packed_cornors_mask;
	image_t packed_wang_tiles;
	image_t graphcut_constraints;
};

resultset_t processimage(image_t image, int debug_tileindex)
{
	resultset_t result;

	wangtiles_t wangtiles(image, 2);
	wangtiles.set_debug_tileindex(debug_tileindex);
	wangtiles.pick_colored_patches();
	wangtiles.generate_packed_cornors();
	wangtiles.generate_wang_tiles();

	result.packed_cornors = wangtiles.get_packed_cornors();
	result.packed_cornors_mask = wangtiles.get_packed_cornors_mask();
	result.packed_wang_tiles = wangtiles.get_packed_wang_tiles();
	result.graphcut_constraints = wangtiles.get_graphcut_constraints();
	return result;
}

int print_usage_on_error()
{
	const char *usage_msg = "Usage:  wtgcore --tiles <resolution> <input-path> <output-path> <output-cornors-path> <output-constraints-path> <debug-tile-index>\n"
							"     |  wtgcore --index <resolution> <output-path>\n";
	std::cerr << usage_msg;
	return -1;
}

int generate_tiles_entry(int argc, const char *argv[])
{
	if (argc < 7) return print_usage_on_error();
	int resolution = std::atoi(argv[2]);
	if (resolution <= 0 || (resolution & (resolution - 1)) != 0)
	{
		std::cerr << "resolution is invalid, must be a POT\n";
		return print_usage_on_error();
	}
	const char *inputpath = argv[3];
	const char *outputpath = argv[4];
	const char *outputpath_cornors = argv[5];
	const char *outputpath_constraints = argv[6];
	int debug_tileindex = -1;
	if (argc > 7) debug_tileindex = std::atoi(argv[7]);

	image_t input;
	input.resolution = resolution;
	if (!(input.pixels = readfile(inputpath, resolution)))
	{
		std::cerr << "read input file failed\n";
		return -1;
	}
	resultset_t result = processimage(input, debug_tileindex);
	if (!writefile(outputpath, result.packed_wang_tiles.pixels, resolution))
	{
		std::cerr << "write output file failed\n";
		return -1;
	}
	if (!writefile(outputpath_cornors, result.packed_cornors.pixels, result.packed_cornors_mask.pixels, resolution))
	{
		std::cerr << "write output cornors file failed\n";
		return -1;
	}
	if (!writefile(outputpath_constraints, result.graphcut_constraints.pixels, result.graphcut_constraints.resolution))
	{
		std::cerr << "write graphcut constraints file failed\n";
		return -1;
	}
	return 0;
}

int generate_indexmap_entry(int argc, const char *argv[])
{
	if (argc != 4) return print_usage_on_error();
	int resolution = std::atoi(argv[2]);
	if (resolution <= 0)
	{
		std::cerr << "resolution is invalid\n";
		return print_usage_on_error();
	}
	const char *outputpath = argv[3];

	wangtiles_t wangtiles(image_t(), 2); // create a wangtiles object with a dummy source image
	image_t indexmap = wangtiles.generate_indexmap(resolution);
	if (!writefile(outputpath, indexmap.pixels, resolution))
	{
		std::cerr << "write output file failed\n";
		return -1;
	}
	return 0;
}

int main(int argc, const char *argv[])
{
	bool generate_indexmap = argc > 1 && strcmp(argv[1], "--index") == 0;
	bool generate_tiles = argc > 1 && strcmp(argv[1], "--tiles") == 0;
	if (generate_indexmap)
		return generate_indexmap_entry(argc, argv);
	else if (generate_tiles)
		return generate_tiles_entry(argc, argv);
	else
		return print_usage_on_error();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
