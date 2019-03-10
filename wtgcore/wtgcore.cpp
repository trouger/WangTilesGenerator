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
	size_t r = fread((void *)data, sizeof(color_t), pixel_count, f);
	fclose(f);
	if (r == pixel_count)
		return data;
	else 
	{
		delete data;
		return NULL;
	}
}

bool writefile(const char *path, const color_t *data, int resolution)
{
	FILE *f;
	if (fopen_s(&f, path, "wb")) return false;
	size_t pixel_count = resolution * resolution;
	size_t r = fwrite(data, sizeof(color_t), pixel_count, f);
	fclose(f);
	if (r == pixel_count)
		return true;
	else
		return false;
}

struct resultset_t
{
	image_t packed_cornors;
	image_t packed_cornors_mask;
	image_t packed_wang_tiles;
	image_t graphcut_constraints;
};

resultset_t processimage(image_t image)
{
	resultset_t result;

	wangtiles_t wangtiles(image, 2);
	wangtiles.pick_colored_patches();
	wangtiles.generate_packed_cornors();
	wangtiles.generate_wang_tiles();

	result.packed_cornors = wangtiles.get_packed_cornors();
	result.packed_cornors_mask = wangtiles.get_packed_cornors_mask();
	result.packed_wang_tiles = wangtiles.get_packed_wang_tiles();
	result.graphcut_constraints = wangtiles.get_graphcut_constraints();
	return result;
}

int main(int argc, const char *argv[])
{
	const char *usage_msg = "Usage: wtgcore <resolution> <input-path> <output-path> <output-cornors-path> <output-constraints-path>\n";
	if (argc < 6)
	{
		std::cout << usage_msg;
		return -1;
	}
	//system("pause");
	int resolution = std::atoi(argv[1]);
	if (resolution <= 0 || (resolution & (resolution - 1)) != 0)
	{
		std::cerr << "resolution is invalid, must be a POT\n";
		std::cout << usage_msg;
		return -1;
	}
	const char *inputpath = argv[2];
	const char *outputpath = argv[3];
	const char *outputpath_cornors = argv[4];
	const char *outputpath_constraints = argv[5];

	image_t input;
	input.resolution = resolution;
	if (!(input.pixels = readfile(inputpath, resolution)))
	{
		std::cerr << "read input file failed\n";
		return -1;
	}
	resultset_t result = processimage(input);
	if (!writefile(outputpath, result.packed_wang_tiles.pixels, resolution))
	{
		std::cerr << "write output file failed\n";
		return -1;
	}
	if (!writefile(outputpath_cornors, result.packed_cornors_mask.pixels, resolution))
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

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
