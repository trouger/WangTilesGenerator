#pragma once

#include <vector>
#include "common_types.h"

struct node_t;
const float infinite_capacity = -1.0f;

struct edge_t
{
	node_t *node;
	float capacity;
	float flow;
	edge_t *inv_edge;

	edge_t(node_t *node, float capacity)
		:node(node), capacity(capacity), flow(0), inv_edge(NULL) { }
};

struct node_t
{
	std::vector<edge_t> neighbors;
	// temp for bfs
	node_t *prev;
	edge_t *prev_edge;

	node_t() :prev(NULL), prev_edge(NULL) { }
};

struct graph_t
{
	std::vector<node_t> nodes;
};

class graphcut_t
{
public:
	graphcut_t(image_t image_a, patch_t patch_a, image_t image_b, patch_t patch_b);
	~graphcut_t();

	void compute_cut_mask(image_t mask_image, patch_t mask_patch);

private:
	node_t &get_pixel_node(int x, int y) { return graph.nodes[y * patch_size + x]; }
	node_t &get_source_node() { return graph.nodes[graph.nodes.size() - 2]; }
	node_t &get_sink_node() { return graph.nodes[graph.nodes.size() - 1]; }

	void make_edge(int x0, int y0, int x1, int y1);
	void make_edge(int x, int y, node_t &src_or_sink);

	void bfs(bool stop_on_sink);

private:
	image_t image_a;
	patch_t patch_a;
	image_t image_b;
	patch_t patch_b;

	graph_t graph;
	int patch_size;
};
