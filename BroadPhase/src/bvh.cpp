#include "bvh.h"
#include <algorithm>
#include <numeric>

void BVH::clear() {
    nodes_.clear();
    max_depth_ = 0;
}

void BVH::build(const std::vector<AABB>& aabbs) {
    clear();
    if (aabbs.empty()) return;

    std::vector<int> indices(aabbs.size());
    std::iota(indices.begin(), indices.end(), 0);

    nodes_.reserve(2 * aabbs.size());
    build_recursive(indices, aabbs, 0);
}

int BVH::build_recursive(std::vector<int>& indices, const std::vector<AABB>& aabbs, int depth) {
    if (depth > max_depth_) max_depth_ = depth;

    int node_idx = static_cast<int>(nodes_.size());
    nodes_.push_back({});

    // Compute bounding box of all shapes in this subset
    AABB bounds = aabbs[indices[0]];
    for (size_t i = 1; i < indices.size(); ++i)
        bounds = bounds.merged(aabbs[indices[i]]);

    if (indices.size() == 1) {
        nodes_[node_idx].bounds = bounds;
        nodes_[node_idx].depth = depth;
        nodes_[node_idx].shape_index = indices[0];
        nodes_[node_idx].subtree_size = 1;
        return node_idx;
    }

    // Choose split axis: longest extent
    float w = bounds.width();
    float h = bounds.height();
    bool split_x = (w >= h);

    // Sort by centroid along chosen axis
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        if (split_x)
            return aabbs[a].center().x < aabbs[b].center().x;
        else
            return aabbs[a].center().y < aabbs[b].center().y;
    });

    // Median split
    size_t mid = indices.size() / 2;
    std::vector<int> left_indices(indices.begin(), indices.begin() + mid);
    std::vector<int> right_indices(indices.begin() + mid, indices.end());

    int left_idx = build_recursive(left_indices, aabbs, depth + 1);
    int right_idx = build_recursive(right_indices, aabbs, depth + 1);

    // Set node data (must be after recursive calls due to vector reallocation)
    nodes_[node_idx].bounds = bounds;
    nodes_[node_idx].depth = depth;
    nodes_[node_idx].left = left_idx;
    nodes_[node_idx].right = right_idx;
    nodes_[node_idx].subtree_size = nodes_[left_idx].subtree_size +
                                     nodes_[right_idx].subtree_size;

    return node_idx;
}

// Self-query: find all overlapping leaf pairs within a subtree or between two subtrees
void BVH::self_query(int a, int b, std::vector<std::pair<int,int>>& pairs) const {
    if (a < 0 || b < 0) return;
    const BVHNode& na = nodes_[a];
    const BVHNode& nb = nodes_[b];

    if (!na.bounds.overlaps(nb.bounds)) return;

    bool a_leaf = (na.shape_index >= 0);
    bool b_leaf = (nb.shape_index >= 0);

    if (a_leaf && b_leaf) {
        int si = na.shape_index, sj = nb.shape_index;
        if (si != sj) {
            if (si > sj) std::swap(si, sj);
            pairs.push_back({si, sj});
        }
        return;
    }

    if (a == b) {
        // Same node: query left vs left, right vs right, left vs right
        self_query(na.left, na.left, pairs);
        self_query(na.right, na.right, pairs);
        self_query(na.left, na.right, pairs);
        return;
    }

    // Different nodes: descend the larger one
    if (a_leaf) {
        self_query(a, nb.left, pairs);
        self_query(a, nb.right, pairs);
    } else if (b_leaf) {
        self_query(na.left, b, pairs);
        self_query(na.right, b, pairs);
    } else {
        if (na.subtree_size >= nb.subtree_size) {
            self_query(na.left, b, pairs);
            self_query(na.right, b, pairs);
        } else {
            self_query(a, nb.left, pairs);
            self_query(a, nb.right, pairs);
        }
    }
}

std::vector<std::pair<int,int>> BVH::find_all_pairs() const {
    std::vector<std::pair<int,int>> pairs;
    if (nodes_.empty()) return pairs;
    self_query(0, 0, pairs);
    return pairs;
}

std::vector<int> BVH::query(const AABB& query_box, int exclude_index) const {
    std::vector<int> results;
    if (nodes_.empty()) return results;

    std::vector<int> stack;
    stack.push_back(0);

    while (!stack.empty()) {
        int idx = stack.back();
        stack.pop_back();
        if (idx < 0) continue;

        const BVHNode& n = nodes_[idx];
        if (!n.bounds.overlaps(query_box)) continue;

        if (n.shape_index >= 0) {
            if (n.shape_index != exclude_index)
                results.push_back(n.shape_index);
        } else {
            stack.push_back(n.left);
            stack.push_back(n.right);
        }
    }
    return results;
}

std::vector<TraversalStep> BVH::query_with_steps(const AABB& query_box, int query_index) const {
    std::vector<TraversalStep> steps;
    if (nodes_.empty()) return steps;

    std::vector<int> stack;
    stack.push_back(0);

    while (!stack.empty()) {
        int idx = stack.back();
        stack.pop_back();
        if (idx < 0) continue;

        const BVHNode& n = nodes_[idx];

        if (!n.bounds.overlaps(query_box)) {
            steps.push_back({idx, TraversalAction::Prune, query_index, -1});
            continue;
        }

        if (n.shape_index >= 0) {
            if (n.shape_index != query_index) {
                steps.push_back({idx, TraversalAction::LeafTest, query_index, n.shape_index});
            }
        } else {
            steps.push_back({idx, TraversalAction::Visit, query_index, -1});
            stack.push_back(n.right);
            stack.push_back(n.left);
        }
    }
    return steps;
}

std::vector<std::pair<int,int>> brute_force_pairs(const std::vector<AABB>& aabbs) {
    std::vector<std::pair<int,int>> pairs;
    int n = static_cast<int>(aabbs.size());
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (aabbs[i].overlaps(aabbs[j]))
                pairs.push_back({i, j});
    return pairs;
}
