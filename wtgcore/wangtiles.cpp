#include "pch.h"
#include <iostream>
#include <mutex>
#include "wangtiles.h"
#include "graphcut.h"
#include "jobsystem.h"

// This is from Figure 9 of the paper "An Alternative for Wang Tiles: Colored Edges versus Colored Corners".
// Four corner colors are encoded as 0, 1, 2, 3.
// A tile is encoded as a base-4 number with 4 digits, which are the colors of the four corners.
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
	:source_image(source), num_colors(num_colors), debug_tileindex(-1)
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
	patch.size = tile_size;
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

void wangtiles_t::generate_packed_corners()
{
	const int num_tiles = num_colors * num_colors;
	const int patch_size = colored_patches[0].size;
	const int tile_size = patch_size;
	const int half_tile_size = tile_size >> 1;
	const int resolution = source_image.resolution;
	packed_corners.clear();
	packed_corners.init(resolution);
	color_t *pixels = packed_corners.pixels;
	for (int cne = 0; cne < num_colors; cne++) {
		for (int cse = 0; cse < num_colors; cse++) {
			for (int csw = 0; csw < num_colors; csw++) {
				for (int cnw = 0; cnw < num_colors; cnw++)
				{
					int corners[4] = { csw, cse, cnw, cne };
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
							int color = corners[(y_north_half << 1) | x_east_half];
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

image_t downsample(const image_t &input)
{
	image_t output;
	output.init(input.resolution >> 1);
	for (int y = 0; y < output.resolution; y++)
	{
		for (int x = 0; x < output.resolution; x++)
		{
			vector3f_t v = get_vector3f(input.get_pixel(x << 1, y << 1));
			v = v + get_vector3f(input.get_pixel((x << 1) + 1, y << 1));
			v = v + get_vector3f(input.get_pixel(x << 1, (y << 1) + 1));
			v = v + get_vector3f(input.get_pixel((x << 1) + 1, (y << 1) + 1));
			color_t c = get_color(v * 0.25f);
			output.set_pixel(x, y, c);
		}
	}
	return output;
}

template <typename _img_t>
_img_t upsample(const _img_t &input)
{
	_img_t output;
	output.init(input.resolution << 1);
	for (int y = 0; y < input.resolution; y++)
	{
		for (int x = 0; x < input.resolution; x++)
		{
			typename _img_t::_pixel_t c = input.get_pixel(x, y);
			output.set_pixel(x << 1, y << 1, c);
			output.set_pixel((x << 1) + 1, y << 1, c);
			output.set_pixel(x << 1, (y << 1) + 1, c);
			output.set_pixel((x << 1) + 1, (y << 1) + 1, c);
		}
	}
	return output;
}

void wangtiles_t::generate_wang_tiles()
{
	const int resolution = source_image.resolution;
	int visual_scale = 128; // apply computer vision processes under a certain scale

	int num_tiles = num_colors * num_colors;
	int tile_size = resolution / num_tiles;
	visual_scale = std::min(visual_scale, tile_size);

	// downsample images into specific visual scale
	int downsample_iterations = 0;
	std::vector<image_t> source_mips, corners_mips;
	source_mips.push_back(source_image);
	corners_mips.push_back(packed_corners);
	while ((tile_size >> downsample_iterations) > visual_scale)
	{
		source_mips.push_back(downsample(source_mips.back()));
		corners_mips.push_back(downsample(corners_mips.back()));
		downsample_iterations++;
	}
	if (source_mips.back().resolution != visual_scale * num_tiles)
	{
		std::cerr << "invalid state\n";
		exit(-1);
	}

	// perform graphcut in visual scale
	graphcut_constraints.clear();
	graphcut_constraints.init(visual_scale);
	fill_graphcut_constraints(visual_scale, graphcut_constraints);
	graphcut_textures(corners_mips.back(), source_mips.back(), graphcut_constraints, packed_corners_mask);

	for (int i = 1; i <= downsample_iterations; i++)
	{
		source_mips[i].clear();
		corners_mips[i].clear();

		packed_corners_mask = upsample(packed_corners_mask);
	}

	// blend the two layers
	std::cout << "blending layers\n";
	packed_wang_tiles.clear();
	packed_wang_tiles.init(resolution);
	for (int y = 0; y < resolution; y++)
	{
		for (int x = 0; x < resolution; x++)
		{
			vector3f_t color0 = get_vector3f(source_image.get_pixel(x, y));
			vector3f_t color1 = get_vector3f(packed_corners.get_pixel(x, y));
			float mask = packed_corners_mask.get_pixel(x, y) / 255.0f;
			vector3f_t color = color0 * (1.0f - mask) + color1 * mask;
			packed_wang_tiles.set_pixel(x, y, get_color(color));
		}
	}
}

image_t wangtiles_t::generate_indexmap(int resolution)
{
	image_t indexmap;
	indexmap.init(resolution);

	image_t cornermap;
	cornermap.init(resolution + 1);
	for (int y = 0; y < resolution; y++)
	{
		for (int x = 0; x < resolution; x++)
		{
			int colorindex = (int)((rand() / (float)(RAND_MAX + 1)) * num_colors);
			cornermap.set_pixel(x, y, color_t(colorindex, 0, 0));
		}
		cornermap.set_pixel(resolution, y, cornermap.get_pixel(0, y));
	}
	for (int x = 0; x <= resolution; x++)
		cornermap.set_pixel(x, resolution, cornermap.get_pixel(x, 0));

	for (int y = 0; y < resolution; y++)
	{
		for (int x = 0; x < resolution; x++)
		{
			int cne = cornermap.get_pixel(x + 1, y + 1).r;
			int cse = cornermap.get_pixel(x + 1, y).r;
			int csw = cornermap.get_pixel(x, y).r;
			int cnw = cornermap.get_pixel(x, y + 1).r;
			int tileindex = inv_packing_table[(cne << 6) | (cse << 4) | (csw << 2) | cnw];
			indexmap.set_pixel(x, y, color_t(tileindex, tileindex, tileindex));
		}
	}
	return indexmap;
}

void wangtiles_t::fill_graphcut_constraints(const int tile_size, image_t &graphcut_constraints)
{
	const int half_tile_size = tile_size >> 1;

	for (int i = 0; i < tile_size * tile_size; i++)
		graphcut_constraints.pixels[i] = CONSTRAINT_COLOR_FREE;

	// must-have constraints
	for (int p = 0; p < tile_size; p++)
	{
		graphcut_constraints.set_pixel(p, 0, CONSTRAINT_COLOR_SOURCE);
		graphcut_constraints.set_pixel(p, tile_size - 1, CONSTRAINT_COLOR_SOURCE);
		if (p == 0 || p == tile_size - 1) continue;

		graphcut_constraints.set_pixel(0, p, CONSTRAINT_COLOR_SOURCE);
		graphcut_constraints.set_pixel(tile_size - 1, p, CONSTRAINT_COLOR_SOURCE);

		graphcut_constraints.set_pixel(p, half_tile_size - 1, CONSTRAINT_COLOR_SINK);
		graphcut_constraints.set_pixel(p, half_tile_size, CONSTRAINT_COLOR_SINK);
		if (p == half_tile_size - 1 || p == half_tile_size) continue;

		graphcut_constraints.set_pixel(half_tile_size - 1, p, CONSTRAINT_COLOR_SINK);
		graphcut_constraints.set_pixel(half_tile_size, p, CONSTRAINT_COLOR_SINK);
	}

	// additional constraints
	int padding = tile_size / 7;
	for (int y = padding; y < tile_size - padding; y++)
		for (int x = padding; x < tile_size - padding; x++)
			graphcut_constraints.set_pixel(x, y, CONSTRAINT_COLOR_SINK);
}

void wangtiles_t::graphcut_textures(image_t image_a, image_t image_b, image_t constraints, mask_t &out_mask)
{
	const int resolution = image_a.resolution;
	const int num_tiles = num_colors * num_colors;
	const int tile_size = resolution / num_tiles;

	out_mask.clear();
	out_mask.init(resolution);

	jobsystem_t jobsystem;
	std::mutex mutex;
	std::vector<algorithm_statistics_t> statistics(num_tiles * num_tiles);
	for (int row = 0; row < num_tiles; row++)
	{
		for (int col = 0; col < num_tiles; col++)
		{
			int tileindex = row * num_tiles + col;
			if (debug_tileindex != -1 && tileindex != debug_tileindex) continue;
			jobsystem.addjob([=, &mutex, &statistics]()
			{
				mutex.lock();
				std::cout << "calculating graphcut for tile " << tileindex << " of " << num_tiles * num_tiles << "\n";
				mutex.unlock();
				patch_t patch;
				patch.size = tile_size;
				patch.x = col * tile_size;
				patch.y = row * tile_size;
				graphcut_t graphcut(image_a, patch, image_b, patch, constraints);
				graphcut.compute_cut_mask(out_mask, patch, statistics[tileindex]);
			});
		}
	}
	jobsystem.startjobs();
	jobsystem.wait();

	for (int i = 0; i < statistics.size(); i++)
	{
		auto stat = statistics[i];
		std::cout << "found max-flow for tile " << i << " after " << stat.iteration_count << " iterations: " << stat.max_flow << std::endl;
	}
}
