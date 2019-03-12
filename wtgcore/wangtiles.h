#pragma once

#include <vector>
#include "common_types.h"

class wangtiles_t
{
public:
	wangtiles_t(image_t source, int num_colors);
	~wangtiles_t();

	void set_debug_tileindex(int tileindex) { debug_tileindex = tileindex; }

	void pick_colored_patches();
	void generate_packed_corners();
	void generate_wang_tiles();

	image_t get_packed_corners() { return packed_corners; }
	mask_t get_packed_corners_mask() { return packed_corners_mask; }
	image_t get_packed_wang_tiles() { return packed_wang_tiles; }
	image_t get_graphcut_constraints() { return graphcut_constraints; }

	image_t generate_indexmap(int resolution);

private:
	void fill_graphcut_constraints(const int tile_size, image_t &constraints);
	void graphcut_textures(image_t image_a, image_t image_b, image_t constraints, mask_t &out_mask);

private:
	image_t source_image;
	int num_colors;
	int inv_packing_table[256];

	std::vector<patch_t> colored_patches;
	image_t packed_corners;
	mask_t packed_corners_mask;
	image_t packed_wang_tiles;
	image_t graphcut_constraints;

	int debug_tileindex;
};

