#pragma once

#include <vector>
#include "common_types.h"

class wangtiles_t
{
public:
	wangtiles_t(image_t source, int num_colors, bool corner_tiles);
	~wangtiles_t();

	void set_debug_tileindex(int tileindex) { debug_tileindex = tileindex; }

	void pick_colored_patches();
	void generate_packed_corners();
	void generate_wang_tiles();

	image_t get_packed_corners() { return packed_corners; }
	mask_t get_packed_corners_mask() { return packed_corners_mask; }
	image_t get_graphcut_constraints() { return graphcut_constraints; }

	image_t generate_indexmap(int resolution);
	image_t generate_palette(int resolution);

private:
	patch_t random_non_overlapping_patch(int patch_size);
	int get_packing_tileindex(int n, int e, int s, int w);
	int random_color();
	void fill_graphcut_constraints(const int tile_size, image_t &constraints);
	void graphcut_textures(image_t image_a, image_t image_b, image_t constraints, mask_t &out_mask);

private:
	bool is_corner_tiles;

	image_t source_image;
	int num_colors;
	int inv_packing_table[256];

	std::vector<patch_t> colored_patches_h;
	std::vector<patch_t> colored_patches_v;
	image_t packed_corners;
	mask_t packed_corners_mask;
	image_t graphcut_constraints;

	int debug_tileindex;
};

