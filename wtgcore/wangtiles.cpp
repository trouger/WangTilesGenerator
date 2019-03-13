#include "pch.h"
#include <iostream>
#include <mutex>
#include "wangtiles.h"
#include "graphcut.h"
#include "jobsystem.h"

// generate a random integer in the range [0, max - 1].
int rand_range(int max)
{
	return int(rand() / (RAND_MAX + 1.0f) * max);
}

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

// if corner_tiles is true, we use the alternative for wang tiles as proposed by the paper "An Alternative for Wang Tiles: Colored Edges versus Colored Corners".
// otherwise we use wang tiles with methods proposed by the paper "Efficient Texture Synthesis Using Strict Wang Tiles".
wangtiles_t::wangtiles_t(image_t source, int num_colors, bool corner_tiles)
	:is_corner_tiles(corner_tiles), source_image(source), num_colors(num_colors), debug_tileindex(-1)
{
	if (num_colors < 2 || num_colors > 4)
	{
		std::cerr << "num_colors must be 2, 3, or 4.\n";
		exit(-1);
	}
	if (is_corner_tiles)
		generate_inv_packing_table(inv_packing_table, num_colors);
}


wangtiles_t::~wangtiles_t()
{
}

// for wang tiles, horizontal and vertical colored patches are picked.
// for corner tiles, only horizontal colored patches are picked to be used as colored corner patches.
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
	colored_patches_h.clear();
	colored_patches_v.clear();

	if (is_corner_tiles)
	{
		patch_t patch;
		patch.size = tile_size;
		patch.x = patch.y = 0;
		colored_patches_h.push_back(patch);

		patch.x = patch.y = resolution - patch.size;
		colored_patches_h.push_back(patch);

		if (num_colors > 2)
		{
			patch.x = 0;
			patch.y = resolution - patch.size;
			colored_patches_h.push_back(patch);
		}
		if (num_colors > 3)
		{
			patch.x = resolution - patch.size;
			patch.y = 0;
			colored_patches_h.push_back(patch);
		}
	}
	else
	{
		// the referenced paper for wang tiles picks diamond-shaped sub-images as colored edge patches.
		// instead, we pick axis-aligned bounding boxes of the diamonds for convenience of representation.
		for (int i = 0; i < num_colors; i++)
			colored_patches_h.push_back(random_non_overlapping_patch(tile_size));
		for (int i = 0; i < num_colors; i++)
			colored_patches_v.push_back(random_non_overlapping_patch(tile_size));
	}
}

void wangtiles_t::generate_packed_corners()
{
	const int num_tiles = num_colors * num_colors;
	const int patch_size = colored_patches_h[0].size;
	const int tile_size = patch_size;
	const int half_tile_size = tile_size >> 1;
	const int resolution = source_image.resolution;
	packed_corners.clear();
	packed_corners.init(resolution);

	if (is_corner_tiles)
	{
		color_t *pixels = packed_corners.pixels;
		for (int cne = 0; cne < num_colors; cne++) for (int cse = 0; cse < num_colors; cse++) for (int csw = 0; csw < num_colors; csw++) for (int cnw = 0; cnw < num_colors; cnw++)
		{
			int corners[4] = { csw, cse, cnw, cne };
			int tileindex = get_packing_tileindex(cne, cse, csw, cnw);
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
					const patch_t &source_patch = colored_patches_h[color];
					int sample_y = y + (1 - y_north_half * 2) * half_tile_size + source_patch.y;
					int sample_x = x + (1 - x_east_half * 2) * half_tile_size + source_patch.x;
					color_t sample = source_image.pixels[sample_y * resolution + sample_x];
					pixels[(y + oy) * resolution + x + ox] = sample;
				}
			}
		}
	}
	else
	{
		memset(packed_corners.pixels, 0, sizeof(color_t) * resolution * resolution);
		auto setpixel_additive = [this](const patch_t &patch, int x, int y, color_t color, float weight)
		{
			vector3f_t src = get_vector3f(packed_corners.get_pixel_in_patch(patch, x, y));
			vector3f_t dst = get_vector3f(color);
			packed_corners.set_pixel_in_patch(patch, x, y, get_color(src + dst * weight));
		};
		for (int n = 0; n < num_colors; n++) for (int e = 0; e < num_colors; e++) for (int s = 0; s < num_colors; s++) for (int w = 0; w < num_colors; w++)
		{
			int tileindex = get_packing_tileindex(n, e, s, w);
			int row = tileindex / num_tiles;
			int col = tileindex - row * num_tiles;
			patch_t dest_patch;
			dest_patch.x = col * tile_size;
			dest_patch.y = row * tile_size;
			dest_patch.size = tile_size;

			const patch_t &ps = colored_patches_h[s];
			const patch_t &pn = colored_patches_h[n];
			const patch_t &pe = colored_patches_v[e];
			const patch_t &pw = colored_patches_v[w];

			// fill the tile by pixels from four colored edge patches
			// by iterating over contributing pixels on four patches simultaneously.
			// the row,col notations are from the upper half of south patch's perspective.
			for (int row = 0; row < half_tile_size; row++)
			{
				for (int col = row; col < tile_size - row; col++)
				{
					float weight = (col == row || col == tile_size - row - 1) ? 0.5f : 1.0f;
					color_t c = source_image.get_pixel_in_patch(ps, col, row + half_tile_size);
					setpixel_additive(dest_patch, col, row, c, weight);
					c = source_image.get_pixel_in_patch(pn, col, half_tile_size - 1 - row);
					setpixel_additive(dest_patch, col, tile_size - 1 - row, c, weight);
					c = source_image.get_pixel_in_patch(pe, half_tile_size - 1 - row, col);
					setpixel_additive(dest_patch, tile_size - 1 - row, col, c, weight);
					c = source_image.get_pixel_in_patch(pw, half_tile_size + row, col);
					setpixel_additive(dest_patch, row, col, c, weight);
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
}

image_t wangtiles_t::generate_indexmap(int resolution)
{
	image_t indexmap;
	indexmap.init(resolution);

	if (is_corner_tiles)
	{
		image_t cornermap;
		cornermap.init(resolution + 1);
		for (int y = 0; y < resolution; y++)
		{
			for (int x = 0; x < resolution; x++)
			{
				cornermap.set_pixel(x, y, color_t(random_color(), 0, 0));
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
				int tileindex = get_packing_tileindex(cne, cse, csw, cnw);
				indexmap.set_pixel(x, y, color_t(tileindex, tileindex, tileindex));
			}
		}
	}
	else
	{
		std::vector<int> bottom(resolution);
		std::vector<int> prev_row(resolution);
		int leftmost_edge = -1;
		int prev_edge = -1;
		int s, w, n, e;
		// first row
		for (int x = 0; x < resolution; x++)
		{
			bottom[x] = s = random_color();
			w = x > 0 ? prev_edge : (leftmost_edge = random_color());
			prev_row[x] = n = resolution > 1 ? random_color() : s;
			prev_edge = e = x < resolution - 1 ? random_color() : leftmost_edge;
			int tileindex = get_packing_tileindex(n, e, s, w);
			indexmap.set_pixel(x, 0, color_t(tileindex, tileindex, tileindex));
		}
		// remaining rows
		for (int y = 1; y < resolution; y++)
		{
			for (int x = 0; x < resolution; x++)
			{
				s = prev_row[x];
				w = x > 0 ? prev_edge : (leftmost_edge = random_color());
				prev_row[x] = n = y < resolution - 1 ? random_color() : bottom[x];
				prev_edge = e = x < resolution - 1 ? random_color() : leftmost_edge;
				int tileindex = get_packing_tileindex(n, e, s, w);
				indexmap.set_pixel(x, y, color_t(tileindex, tileindex, tileindex));
			}
		}
	}
	return indexmap;
}

template <typename _t>
_t lerp(_t a, _t b, float k)
{
	return a * (1.0f - k) + b * k;
}

template <typename _t>
_t smoothlerp(_t a, _t b, float k)
{
	k = -cos(3.1415926f * k) * 0.5f + 0.5f;
	return lerp(a, b, k);
}

image_t wangtiles_t::generate_palette(const int resolution)
{
	const int num_tiles = num_colors * num_colors;
	const int tile_size = resolution / num_tiles;
	if (tile_size * num_tiles != resolution)
	{
		std::cerr << "resolution must be a multiple of num_colors * num_colors\n";
		exit(-1);
	}

	if (is_corner_tiles)
	{
		return image_t(); // not implemented
	}
	else
	{
		const vector3f_t edgecolor_h[] = { get_vector3f(color_t(30, 129, 43)), get_vector3f(color_t(168, 44, 34)) };
		const vector3f_t edgecolor_v[] = { get_vector3f(color_t(24, 98, 169)), get_vector3f(color_t(236, 178, 0)) };

		image_t palette;
		palette.init(resolution);
		for (int n = 0; n < num_colors; n++) for (int e = 0; e < num_colors; e++) for (int s = 0; s < num_colors; s++) for (int w = 0; w < num_colors; w++)
		{
			int tileindex = get_packing_tileindex(n, e, s, w);
			int row = tileindex / num_tiles;
			int col = tileindex - row * num_tiles;
			patch_t dest_patch;
			dest_patch.x = col * tile_size;
			dest_patch.y = row * tile_size;
			dest_patch.size = tile_size;

			for (int y = 0; y < tile_size; y++)
			{
				for (int x = 0; x < tile_size; x++)
				{
					float factor_h = (x + 0.5f) / tile_size;
					float factor_v = (y + 0.5f) / tile_size;
					vector3f_t color_h = smoothlerp(edgecolor_h[w], edgecolor_h[e], factor_h);
					vector3f_t color_v = smoothlerp(edgecolor_v[s], edgecolor_v[n], factor_v);
					factor_h = std::min(factor_h, 1.0f - factor_h);
					factor_v = std::min(factor_v, 1.0f - factor_v);
					float normalize_base = factor_h + factor_v;
					factor_h /= normalize_base;
					factor_v /= normalize_base;
					vector3f_t color = factor_h < factor_v ? smoothlerp(color_h, color_v, factor_h) : smoothlerp(color_v, color_h, factor_v);
					palette.set_pixel_in_patch(dest_patch, x, y, get_color(color));
				}
			}
		}
		return palette;
	}
}

int packing_index_1d(int e1, int e2)
{
	if (e1 == e2)
		return e2 > 0 ? (e1 + 1) * (e1 + 1) - 2 : 0;
	else if (e1 > e2)
		return e2 > 0 ? e1 * e1 + 2 * e2 - 1 : (e1 + 1) * (e1 + 1) - 1;
	else
		return 2 * e1 + e2 * e2;
}

patch_t wangtiles_t::random_non_overlapping_patch(int patch_size)
{
	auto check_overlap = [](const patch_t &p0, const patch_t &p1)
	{
		int min_x = std::min(p0.x, p1.x);
		int max_x = std::max(p0.x + p0.size, p1.x + p1.size);
		int min_y = std::min(p0.y, p1.y);
		int max_y = std::max(p0.y + p0.size, p1.y + p1.size);
		int bounding_size_x = max_x - min_x;
		int bounding_size_y = max_y - min_y;
		return std::max(bounding_size_x, bounding_size_y) < p0.size + p1.size;
	};

	const int resolution = source_image.resolution;
	while (1)
	{
		patch_t newpatch;
		newpatch.size = patch_size;
		newpatch.x = rand_range(resolution - patch_size + 1);
		newpatch.y = rand_range(resolution - patch_size + 1);
		bool overlap = false;
		for (auto it = colored_patches_h.cbegin(); it != colored_patches_h.cend(); it++)
			if ((overlap = check_overlap(newpatch, *it)) == true) break;
		if (overlap) continue;
		for (auto it = colored_patches_v.cbegin(); it != colored_patches_v.cend(); it++)
			if ((overlap = check_overlap(newpatch, *it)) == true) break;
		if (!overlap) return newpatch;
	}
}

// for wang tiles it is (n, e, s, w), for corner tiles it is (ne, se, sw, nw)
int wangtiles_t::get_packing_tileindex(int n, int e, int s, int w)
{
	if (is_corner_tiles)
		return inv_packing_table[(n << 6) | (e << 4) | (s << 2) | w];
	else
	{
		int row = packing_index_1d(s, n);
		int col = packing_index_1d(w, e);
		return row * num_colors * num_colors + col;
	}
}

int wangtiles_t::random_color()
{
	return rand_range(num_colors);
}

void wangtiles_t::fill_graphcut_constraints(const int tile_size, image_t &graphcut_constraints)
{
	const int half_tile_size = tile_size >> 1;

	for (int i = 0; i < tile_size * tile_size; i++)
		graphcut_constraints.pixels[i] = CONSTRAINT_COLOR_FREE;

	if (is_corner_tiles)
	{
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
	else
	{
		// must-have constraints
		for (int p = 0; p < tile_size; p++)
		{
			graphcut_constraints.set_pixel(p, 0, CONSTRAINT_COLOR_SOURCE);
			graphcut_constraints.set_pixel(p, tile_size - 1, CONSTRAINT_COLOR_SOURCE);
			if (p == 0 || p == tile_size - 1) continue;

			graphcut_constraints.set_pixel(0, p, CONSTRAINT_COLOR_SOURCE);
			graphcut_constraints.set_pixel(tile_size - 1, p, CONSTRAINT_COLOR_SOURCE);

			graphcut_constraints.set_pixel(p, p, CONSTRAINT_COLOR_SINK);
			graphcut_constraints.set_pixel(p, tile_size - 1 - p, CONSTRAINT_COLOR_SINK);
		}

		// additional constraints
		int padding = tile_size / 7;
		for (int y = padding; y < tile_size - padding; y++)
			for (int x = padding; x < tile_size - padding; x++)
				graphcut_constraints.set_pixel(x, y, CONSTRAINT_COLOR_SINK);
	}
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
