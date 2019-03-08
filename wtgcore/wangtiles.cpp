#include "pch.h"
#include <iostream>
#include <mutex>
#include "wangtiles.h"
#include "graphcut.h"
#include "jobsystem.h"

// This is from Figure 9 of the paper "An Alternative for Wang Tiles: Colored Edges versus Colored Corners".
// Four cornor colors are encoded as 0, 1, 2, 3.
// A tile is encoded as a base-4 number with 4 digits, which are the colors of the four cornors.
// The 4-digit number is C(NE)C(SE)C(SW)C(NW).
int reference_packing_table[] = {
	0, 16, 68, 1,
	64, 65, 81, 5,
	17, 84, 85, 69,
	4, 80, 21, 20,
};
const int reference_packing_table_size = 4;

void generate_inv_packing_table(int inv_packing_table[], int num_colors)
{
	int packing_table_size = num_colors * num_colors;
	if (reference_packing_table_size < packing_table_size)
	{
		std::cerr << "reference packing table is too small for " << num_colors << " colors\n";
		exit(-1);
	}
	for (int row = 0; row < packing_table_size; row++)
	{
		for (int col = 0; col < packing_table_size; col++)
		{
			int index_in_reference_table = row * reference_packing_table_size + col;
			int index_in_actual_table = row * packing_table_size + col;
			inv_packing_table[reference_packing_table[index_in_reference_table]] = index_in_actual_table;
		}
	}
}

wangtiles_t::wangtiles_t(image_t source, int num_colors)
	:source_image(source), num_colors(num_colors)
{
	if (num_colors < 2 || num_colors > 4)
	{
		std::cerr << "num_colors must be 2, 3, or 4.\n";
		exit(-1);
	}
	generate_inv_packing_table(inv_packing_table, num_colors);
}


wangtiles_t::~wangtiles_t()
{
}

void wangtiles_t::pick_colored_patches()
{
	const int num_tiles = num_colors * num_colors;
	const int resolution = source_image.resolution;
	const int tile_size = resolution / num_tiles;
	if (tile_size * num_tiles != source_image.resolution)
	{
		std::cerr << "input image resolution must be a multiple of num_colors * num_colors\n";
		exit(-1);
	}
	colored_patches.clear();

	patch_t patch;
	patch.size = tile_size * 2;
	patch.x = patch.y = 0;
	colored_patches.push_back(patch);

	patch.x = patch.y = resolution - patch.size;
	colored_patches.push_back(patch);

	if (num_colors > 2)
	{
		patch.x = 0;
		patch.y = resolution - patch.size;
		colored_patches.push_back(patch);
	}
	if (num_colors > 3)
	{
		patch.x = resolution - patch.size;
		patch.y = 0;
		colored_patches.push_back(patch);
	}
}

void wangtiles_t::generate_packed_cornors()
{
	const int num_tiles = num_colors * num_colors;
	const int patch_size = colored_patches[0].size;
	const int tile_size = patch_size >> 1;
	const int half_tile_size = tile_size >> 1;
	const int resolution = source_image.resolution;
	packed_cornors.clear();
	packed_cornors.init(resolution);
	color_t *pixels = packed_cornors.pixels;
	for (int cne = 0; cne < num_colors; cne++) {
		for (int cse = 0; cse < num_colors; cse++) {
			for (int csw = 0; csw < num_colors; csw++) {
				for (int cnw = 0; cnw < num_colors; cnw++)
				{
					int cornors[4] = { csw, cse, cnw, cne };
					int tileindex = inv_packing_table[(cne << 6) | (cse << 4) | (csw << 2) | cnw];
					int row = tileindex / num_tiles;
					int col = tileindex - row * num_tiles;
					int ox = col * tile_size;
					int oy = row * tile_size;
					for (int y = 0; y < tile_size; y++)
					{
						for (int x = 0; x < tile_size; x++)
						{
							int y_north_half = y >= half_tile_size ? 1 : 0;
							int x_east_half = x >= half_tile_size ? 1 : 0;
							int color = cornors[(y_north_half << 1) | x_east_half];
							const patch_t &source_patch = colored_patches[color];
							int sample_y = y + (1 - y_north_half * 2) * half_tile_size + source_patch.y;
							int sample_x = x + (1 - x_east_half * 2) * half_tile_size + source_patch.x;
							color_t sample = source_image.pixels[sample_y * resolution + sample_x];
							pixels[(y + oy) * resolution + x + ox] = sample;
						}
					}
				}
			}
		}
	}
}

void wangtiles_t::generate_wang_tiles()
{
	const int resolution = source_image.resolution;
	const int num_tiles = num_colors * num_colors;
	const int tile_size = resolution / num_tiles;
	
	packed_cornors_mask.clear();
	packed_cornors_mask.init(resolution);

	jobsystem_t jobsystem;
	std::mutex mutex;
	for (int row = 0; row < num_tiles; row++)
	{
		for (int col = 0; col < num_tiles; col++)
		{
			jobsystem.addjob([=, &mutex]()
			{
				mutex.lock();
				std::cout << "calculating graphcut for tile " << row * num_tiles + col << " of " << num_tiles * num_tiles << "\n";
				mutex.unlock();
				patch_t patch;
				patch.size = tile_size;
				patch.x = col * tile_size;
				patch.y = row * tile_size;
				graphcut_t graphcut(packed_cornors, patch, source_image, patch);
				graphcut.compute_cut_mask(packed_cornors_mask, patch); 
			});
		}
	}
	jobsystem.startjobs();
	jobsystem.wait();

	// blend the two layers
	std::cout << "blending layers\n";
	packed_wang_tiles.clear();
	packed_wang_tiles.init(resolution);
	for (int y = 0; y < resolution; y++)
	{
		for (int x = 0; x < resolution; x++)
		{
			vector3f_t color0 = get_vector3f(source_image.get_pixel(x, y));
			vector3f_t color1 = get_vector3f(packed_cornors.get_pixel(x, y));
			float mask = packed_cornors_mask.get_pixel(x, y).r / 255.0f;
			vector3f_t color = color0 * (1.0f - mask) + color1 * mask;
			packed_wang_tiles.set_pixel(x, y, get_color(color));
		}
	}
}
