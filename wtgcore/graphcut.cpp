#include "pch.h"
#include "graphcut.h"
#include <iostream>

// patch a (the source) is put on a layer over patch b (the sink).
// this class generates a best-matching mask of patch a, given the initial constraints.
graphcut_t::graphcut_t(image_t image_a, patch_t patch_a, image_t image_b, patch_t patch_b, image_t constraints)
	:image_a(image_a), patch_a(patch_a), image_b(image_b), patch_b(patch_b)
{
	patch_size = patch_a.size;
	if (patch_size < 2 || patch_size != patch_b.size)
	{
		std::cerr << "invalid patch size\n";
		exit(-1);
	}
	graph.nodes.resize(patch_size * patch_size + 2);
	for (int y = 0; y < patch_size; y++)
	{
		for (int x = 0; x < patch_size; x++)
		{
			if (y > 0) make_edge(x, y, x, y - 1);
			if (y < patch_size - 1) make_edge(x, y, x, y + 1);
			if (x > 0) make_edge(x, y, x - 1, y);
			if (x < patch_size - 1) make_edge(x, y, x + 1, y);
		}
	}
	node_t &source = get_source_node();
	node_t &sink = get_sink_node();

	for (int y = 0; y < patch_size; y++)
	{
		for (int x = 0; x < patch_size; x++)
		{
			color_t constraint = constraints.get_pixel(x, y);
			if (constraint == CONSTRAINT_COLOR_SOURCE)
				make_edge(x, y, source);
			else if (constraint == CONSTRAINT_COLOR_SINK)
				make_edge(x, y, sink);
		}
	}
}

graphcut_t::~graphcut_t()
{
}

void graphcut_t::bfs(bool stop_on_sink)
{
	node_t &source = get_source_node();
	node_t &sink = get_sink_node();
	for (size_t i = 0; i < graph.nodes.size(); i++)
	{
		graph.nodes[i].prev = NULL;
		graph.nodes[i].prev_edge = NULL;
	}

	while (bfs_queue.size() > 0) bfs_queue.pop();
	bfs_queue.push(&source);
	source.prev = &source;

	while (bfs_queue.size() > 0)
	{
		node_t &cur = *bfs_queue.front();
		bfs_queue.pop();
		for (auto it = cur.neighbors.begin(), itend = cur.neighbors.end(); it != itend; ++it)
		{
			edge_t &edge = *it;
			node_t &next = *edge.node;
			if (next.prev != NULL) continue;
			if (edge.capacity == infinite_capacity || edge.flow < edge.capacity)
			{
				next.prev = &cur;
				next.prev_edge = &edge;
				bfs_queue.push(&next);
			}
		}
		if (stop_on_sink && sink.prev != NULL) break;
	}
}

// get a mask which should be applied to patch a
void graphcut_t::compute_cut_mask(image_t mask_image, patch_t mask_patch, algorithm_statistics_t &statistics)
{
	if (patch_size != mask_patch.size)
	{
		std::cerr << "invalid mask patch size\n";
		exit(-1);
	}
	statistics.iteration_count = 0;
	statistics.max_flow = 0;

	// calculate the max flow of the graph
	node_t &source = get_source_node();
	node_t &sink = get_sink_node();
	while (1)
	{
		statistics.iteration_count++;
		// find augmenting path
		bfs(true);
		if (sink.prev == NULL) break;

		// find min flow on this path
		node_t *pnode = &sink;
		float flow = infinite_capacity;
		while (pnode != &source)
		{
			edge_t *edge = pnode->prev_edge;
			float remain_cap = edge->capacity == infinite_capacity ? infinite_capacity : edge->capacity - edge->flow;
			if (flow == infinite_capacity)
				flow = remain_cap;
			else
				flow = remain_cap == infinite_capacity ? flow : std::min(flow, remain_cap);
			pnode = pnode->prev;
		}
		// add this flow
		pnode = &sink;
		while (pnode != &source)
		{
			edge_t &edge = *pnode->prev_edge;
			edge_t &inv_edge = edge.node->neighbors[edge.inv_edge_index];
			edge.flow += flow;
			inv_edge.flow = -edge.flow;
			pnode = pnode->prev;
		}
		statistics.max_flow += flow;
	}
	// find the cut by the reachable set from source in the residual graph
	bfs(false);
	// fill the mask
	for (int y = 0; y < patch_size; y++)
	{
		for (int x = 0; x < patch_size; x++)
		{
			bool reachable = get_pixel_node(x, y).prev != NULL;
			mask_image.set_pixel(x + mask_patch.x, y + mask_patch.y, reachable ? color_t(255, 255, 255) : color_t(0, 0, 0));
		}
	}
}

// this method must guarantee edge weight is symmetric
void graphcut_t::make_edge(int x0, int y0, int x1, int y1)
{
	node_t &node0 = get_pixel_node(x0, y0);
	node_t &node1 = get_pixel_node(x1, y1);
	if (&node0 > &node1) return;

	vector3f_t a0 = get_vector3f(image_a.get_pixel(patch_a.x + x0, patch_a.y + y0));
	vector3f_t a1 = get_vector3f(image_a.get_pixel(patch_a.x + x1, patch_a.y + y1));
	vector3f_t b0 = get_vector3f(image_b.get_pixel(patch_b.x + x0, patch_b.y + y0));
	vector3f_t b1 = get_vector3f(image_b.get_pixel(patch_b.x + x1, patch_b.y + y1));
	float cost = (a0 - b0).magnitude() + (a1 - b1).magnitude() + 1.0f;
	
	node0.neighbors.emplace_back(&node1, cost);
	node1.neighbors.emplace_back(&node0, cost);
	edge_t &edge0 = node0.neighbors.back();
	edge_t &edge1 = node1.neighbors.back();
	edge0.inv_edge_index = node1.neighbors.size() - 1;
	edge1.inv_edge_index = node0.neighbors.size() - 1;
}

void graphcut_t::make_edge(int x, int y, node_t & src_or_sink)
{
	node_t &node = get_pixel_node(x, y);
	node.neighbors.emplace_back(&src_or_sink, infinite_capacity);
	src_or_sink.neighbors.emplace_back(&node, infinite_capacity);

	edge_t &edge0 = node.neighbors.back();
	edge_t &edge1 = src_or_sink.neighbors.back();
	edge0.inv_edge_index = src_or_sink.neighbors.size() - 1;
	edge1.inv_edge_index = node.neighbors.size() - 1;
}
