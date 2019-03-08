#pragma once

#include <vector>
#include "common_types.h"

class wangtiles_t
{
public:
	wangtiles_t(image_t source, int num_colors);
	~wangtiles_t();

	void pick_colored_patches();
	void generate_packed_cornors();
	void generate_wang_tiles();

	image_t get_packed_cornors() { return packed_cornors; }
	image_t get_packed_cornors_mask() { return packed_cornors_mask; }
	image_t get_packed_wang_tiles() { return packed_wang_tiles; }

private:
	image_t source_image;
	int num_colors;
	int inv_packing_table[256];

	std::vector<patch_t> colored_patches;
	image_t packed_cornors;
	image_t packed_cornors_mask;
	image_t packed_wang_tiles;
};

