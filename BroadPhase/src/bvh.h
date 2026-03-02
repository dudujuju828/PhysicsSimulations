#pragma once
#include "aabb.h"
#include <vector>
#include <utility>

struct BVHNode {
    AABB bounds{};
    int  left        = -1;   // Child indices (-1 for leaf)
    int  right       = -1;
    int  shape_index = -1;   // >= 0 only for leaf nodes
    int  depth       = 0;
    int  subtree_size = 1;   // Number of leaves in subtree
};

enum class TraversalAction { Visit, Prune, LeafTest };

struct TraversalStep {
    int             node_index;
    TraversalAction action;
    int             query_shape;    // Shape being queried
    int             partner_shape;  // Partner shape (for LeafTest)
};

class BVH {
public:
    void build(const std::vector<AABB>& aabbs);
    void clear();

    // Find all overlapping pairs via self-query
    std::vector<std::pair<int,int>> find_all_pairs() const;

    // Query for shapes overlapping a given AABB, recording traversal steps
    std::vector<TraversalStep> query_with_steps(const AABB& query, int query_index) const;

    // Query for shape indices overlapping a given AABB
    std::vector<int> query(const AABB& query, int exclude_index) const;

    const std::vector<BVHNode>& nodes() const { return nodes_; }
    int root() const { return nodes_.empty() ? -1 : 0; }
    int max_depth() const { return max_depth_; }

private:
    int build_recursive(std::vector<int>& indices, const std::vector<AABB>& aabbs, int depth);
    void self_query(int nodeA, int nodeB, std::vector<std::pair<int,int>>& pairs) const;

    std::vector<BVHNode> nodes_;
    int max_depth_ = 0;
};

// Brute-force all pairs that have overlapping AABBs
std::vector<std::pair<int,int>> brute_force_pairs(const std::vector<AABB>& aabbs);
