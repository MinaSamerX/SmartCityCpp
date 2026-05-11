#pragma once
// src/spatial/BST.h
// Smart City Delivery & Traffic Management System

#include <memory>
#include <functional>
#include <vector>
#include <ostream>
#include "../core/Package.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: BST<T, KeyFn, Compare>
//
//  Generic Binary Search Tree.
//
//  Purpose in this system:
//    - Maintain a sorted set of Packages ordered by deadline
//    - O(log N) insert / search / remove (balanced on average)
//    - In-order traversal yields packages sorted earliest→latest deadline
//    - Priority-based vehicle assignment: always process closest deadline first
//
//  OOP / Modern C++:
//    - Nodes owned by unique_ptr — automatic memory management (Rule of Zero)
//    - KeyFn policy extracts the comparable key from T (lambda-friendly)
//    - Const-correct: all read queries are const
//    - Supports range queries: all items with key in [lo, hi]
// ─────────────────────────────────────────────────────────────────────────────
template<
    typename T,
    typename Key     = std::time_t,
    typename KeyFn   = std::function<Key(const T&)>,
    typename Compare = std::less<Key>
>
class BST {
private:
    struct Node {
        T                       data;
        std::unique_ptr<Node>   left;
        std::unique_ptr<Node>   right;
        explicit Node(const T& d) : data(d) {}
    };

public:
    // ── Construction ─────────────────────────────────────────────────────────
    explicit BST(KeyFn keyFn  = [](const T& t) -> Key { return static_cast<Key>(t); },
                 Compare cmp  = Compare{})
        : m_root(nullptr)
        , m_size(0)
        , m_keyFn(std::move(keyFn))
        , m_cmp(cmp)
    {}

    // Movable, not copyable (unique_ptr tree)
    BST(const BST&)             = delete;
    BST& operator=(const BST&)  = delete;
    BST(BST&&)                  = default;
    BST& operator=(BST&&)       = default;

    // ── Core Operations ───────────────────────────────────────────────────────

    void insert(const T& item) {
        m_root = insertRec(std::move(m_root), item);
        ++m_size;
    }

    // Returns pointer to found item, nullptr if not found
    const T* search(const Key& key) const {
        return searchRec(m_root.get(), key);
    }

    bool contains(const Key& key) const { return search(key) != nullptr; }

    // Remove by key — returns true if removed
    bool remove(const Key& key) {
        bool removed = false;
        m_root = removeRec(std::move(m_root), key, removed);
        if (removed) --m_size;
        return removed;
    }

    // ── Traversals ────────────────────────────────────────────────────────────

    // In-order: sorted ascending by key
    std::vector<T> inorder() const {
        std::vector<T> result;
        result.reserve(m_size);
        inorderRec(m_root.get(), result);
        return result;
    }

    // Range query: all items where lo <= key <= hi
    std::vector<T> rangeQuery(const Key& lo, const Key& hi) const {
        std::vector<T> result;
        rangeRec(m_root.get(), lo, hi, result);
        return result;
    }

    // Min / Max element
    const T* minimum() const {
        if (!m_root) return nullptr;
        Node* cur = m_root.get();
        while (cur->left) cur = cur->left.get();
        return &cur->data;
    }

    const T* maximum() const {
        if (!m_root) return nullptr;
        Node* cur = m_root.get();
        while (cur->right) cur = cur->right.get();
        return &cur->data;
    }

    // ── Metrics ───────────────────────────────────────────────────────────────
    std::size_t size()  const { return m_size;        }
    bool        empty() const { return m_size == 0;   }
    int         height()const { return heightRec(m_root.get()); }

    void clear() { m_root.reset(); m_size = 0; }

    void print(std::ostream& os) const {
        os << "BST[size=" << m_size << " height=" << height() << "]\n";
        auto items = inorder();
        for (const auto& item : items) os << "  " << item << "\n";
    }

private:
    std::unique_ptr<Node> m_root;
    std::size_t           m_size{0};
    KeyFn                 m_keyFn;
    Compare               m_cmp;

    // ── Recursive helpers ─────────────────────────────────────────────────────
    std::unique_ptr<Node> insertRec(std::unique_ptr<Node> node, const T& item) {
        if (!node) return std::make_unique<Node>(item);
        Key k = m_keyFn(item), nk = m_keyFn(node->data);
        if      (m_cmp(k, nk)) node->left  = insertRec(std::move(node->left),  item);
        else if (m_cmp(nk, k)) node->right = insertRec(std::move(node->right), item);
        else                    node->data  = item; // duplicate key → overwrite
        return node;
    }

    const T* searchRec(const Node* node, const Key& key) const {
        if (!node) return nullptr;
        Key nk = m_keyFn(node->data);
        if      (m_cmp(key, nk)) return searchRec(node->left.get(),  key);
        else if (m_cmp(nk, key)) return searchRec(node->right.get(), key);
        else                     return &node->data;
    }

    std::unique_ptr<Node> removeRec(std::unique_ptr<Node> node,
                                    const Key& key, bool& removed) {
        if (!node) return nullptr;
        Key nk = m_keyFn(node->data);
        if (m_cmp(key, nk)) {
            node->left  = removeRec(std::move(node->left),  key, removed);
        } else if (m_cmp(nk, key)) {
            node->right = removeRec(std::move(node->right), key, removed);
        } else {
            removed = true;
            if (!node->left)  return std::move(node->right);
            if (!node->right) return std::move(node->left);
            // Two children: replace with in-order successor (min of right)
            Node* succ = node->right.get();
            while (succ->left) succ = succ->left.get();
            node->data  = succ->data;
            bool dummy  = false;
            node->right = removeRec(std::move(node->right),
                                    m_keyFn(succ->data), dummy);
        }
        return node;
    }

    void inorderRec(const Node* node, std::vector<T>& out) const {
        if (!node) return;
        inorderRec(node->left.get(),  out);
        out.push_back(node->data);
        inorderRec(node->right.get(), out);
    }

    void rangeRec(const Node* node, const Key& lo,
                  const Key& hi, std::vector<T>& out) const {
        if (!node) return;
        Key nk = m_keyFn(node->data);
        if (!m_cmp(nk, lo))          // nk >= lo → go left
            rangeRec(node->left.get(), lo, hi, out);
        if (!m_cmp(nk, lo) && !m_cmp(hi, nk))  // lo <= nk <= hi
            out.push_back(node->data);
        if (m_cmp(nk, hi))           // nk < hi → go right
            rangeRec(node->right.get(), lo, hi, out);
    }

    int heightRec(const Node* node) const {
        if (!node) return 0;
        return 1 + std::max(heightRec(node->left.get()),
                            heightRec(node->right.get()));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience alias — BST ordered by Package deadline
// ─────────────────────────────────────────────────────────────────────────────
using DeadlineBST = BST<
    std::shared_ptr<Package>,
    std::time_t,
    std::function<std::time_t(const std::shared_ptr<Package>&)>
>;

inline DeadlineBST makeDeadlineBST() {
    return DeadlineBST(
        [](const std::shared_ptr<Package>& p) { return p->getDeadline(); }
    );
}