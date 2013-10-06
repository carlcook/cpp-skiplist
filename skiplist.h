#pragma once

#include <stdlib.h>

#include <memory>
#include <vector>
#include <limits>

namespace nonstd {

// internal to the skiplist
namespace detail {

// the max height that the skiplist can be
const size_t MAX_HEIGHT = 10;

// internal representation of a node within a skiplist
template <class Key, class Allocator>
struct node {
  // the key itself
  const Key mKey;

  // the neighbour to the right of this node, at every level
  std::vector<node*, Allocator> mRhs;

  node(const Key& key, size_t height) : mKey(key), mRhs(height) {}
};

// static container to hold a temp buffer of pointers
template <class A, class Node>
struct pointers_to_node {
  typedef typename A::template rebind<Node*>::other allocator;

  // all the nodes to the left of the target node
  Node** mLhs;

  pointers_to_node() 
      : mLhs(allocator().allocate(MAX_HEIGHT)) {
    srand(time(nullptr));
  }

  ~pointers_to_node() {
    allocator().deallocate(mLhs, MAX_HEIGHT);
  }
};

}

// a skiplist is a cross between a linked list and a self balancing tree,
// expect logarithmic time complexity for insert, find and erase
// expect approximately O(N) space complexity
template <class T, class C = std::less<T>, class A = std::allocator<T>>
class skiplist {
 private:
  typedef typename detail::node<T, A> node_type;

 public:
  typedef A allocator_type;
  typedef C compare_type;
  typedef typename allocator_type::value_type value_type;
  typedef typename allocator_type::size_type size_type;
  typedef typename allocator_type::reference reference;
  typedef typename allocator_type::const_reference const_reference;

  // this is a forward iterator (only)
  // TODO just make this a const iterator
  class iterator {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef typename allocator_type::value_type value_type;
    typedef typename allocator_type::const_reference const_reference;
    typedef typename allocator_type::const_pointer const_pointer;

    typedef typename allocator_type::reference reference;
    typedef typename allocator_type::pointer pointer;
    typedef typename allocator_type::difference_type difference_type;

    // standard says the iterator must be default constructable
    iterator() : mSkiplist(nullptr), mNode(nullptr) {}

    // copy constructor
    iterator(const iterator& other)
        : mSkiplist(other.mSkiplist), mNode(other.mNode) {}

    ~iterator() {
      mSkiplist = nullptr;
      mNode = nullptr;
    }

    // assignment operator
    iterator& operator=(const iterator& other) {
      if (other != *this) {
        mSkiplist = other.mSkiplist;
        mNode = other.mNode;
      }
      return *this;
    }

    bool operator==(const iterator& other) const {
      // iterators are only considered equal if they refer to the same list in
      // memory (not just equivalent lists)
      return mSkiplist == other.mSkiplist && mNode == other.mNode;
    }

    bool operator!=(const iterator& other) const { return !(*this == other); }

    iterator& operator++() {
      mNode = mNode->mRhs[0];
      return *this;
    }

    iterator operator++(int) {
      iterator old(*this);
      mNode = mNode->mRhs[0];
      return old;
    }

    const_reference operator*() const { return mNode->mKey; }

    const_pointer operator->() const { return &mNode->mKey; }

   private:
    // the owning skiplist
    const skiplist* mSkiplist;

    // pointer to the node under iteration
    node_type* mNode;

    // set pointer to head of list (excluding the default node)
    iterator(const skiplist* skiplist, node_type* node = nullptr)
        : mSkiplist(skiplist), mNode(node) {}

   friend class skiplist;
  };

  skiplist(const compare_type& compare = compare_type()) : mHead(T(), detail::MAX_HEIGHT), mCompare(compare) {}

  // TODO just the allocator as an argument to a constructor

  skiplist(const skiplist& other) : mHead(T(), detail::MAX_HEIGHT) {
    // take a copy (not copying network of other skiplist)
    for (auto it = other.begin(); it != other.end(); ++it) insert(*it);
  }

  // TODO initialization list ctor

  // TODO move ctor

  // TODO move assignment operator

  // TODO regular assignment operator

  // TODO initializer list assignment operator

  ~skiplist() { clear(); }

  std::pair<iterator, bool> insert(const T& key) {
    // create random height
    size_t height = 1;
    while (height < detail::MAX_HEIGHT && rand() % 2) height++;

    // build up list, up to given height during find
    discoverLinksToUpdate(key, height);

    // create new node using given allocator
    auto newnode = mNodeAllocator.allocate(1);
    mNodeAllocator.construct(newnode, key, height);

    // splice the new node into the list
    auto lhs = get_pointer_buffer().mLhs;
    for (auto level = height; level; level--) {
      newnode->mRhs[level - 1] = lhs[level - 1]->mRhs[level - 1];
      lhs[level - 1]->mRhs[level - 1] = newnode;
    }
    return std::make_pair(iterator(this, newnode), true);
  }

  iterator find(const T& key) const {
    auto n = &mHead;

    // inspect all ten levels
    for (auto level = mHead.mRhs.size(); level; level--) {
      // drop down when no adjacent node exists
      while (n && n->mRhs[level - 1]) {
        auto keyIsLess = mCompare(key, n->mRhs[level - 1]->mKey);
        if (keyIsLess) break; // drop down

        auto nodeIsLess = mCompare(n->mRhs[level - 1]->mKey, key);
        if (nodeIsLess) {
          n = n->mRhs[level - 1]; // walk across
          continue;
        }
        return iterator(this, n->mRhs[level - 1]);
      }
    }
    return iterator(this, nullptr); // not found
  }

  iterator erase(const iterator position) {
    discoverLinksToUpdate(*position, position.mNode->mRhs.size());

    auto lhs = get_pointer_buffer().mLhs;
    for (auto level = position.mNode->mRhs.size(); level; level--) {
      if (!lhs[level - 1]) continue;
      lhs[level - 1]->mRhs[level - 1]  = position.mNode->mRhs[level - 1];
    }

    iterator next(this, position.mNode->mRhs[0]);
    mNodeAllocator.destroy(position.mNode);
    mNodeAllocator.deallocate(position.mNode, 1);
    return next;
  }

  iterator begin() const { return iterator(this, mHead.mRhs[0]); }

  // the end is represented by a null node under iteration
  iterator end() const { return iterator(this, nullptr); }

  // TODO this isn't really random access just yet ;)
  const_reference operator[](size_type position) const {
    auto n = mHead.mRhs[0];
    while (position-- > 0) n = n->mRhs[0];
    return n->mKey;
  }

  void swap(skiplist& other) { std::swap(mHead.mRhs, other.mHead.mRhs); }

  void clear() {
    auto it = begin();
    while (it != end()) it = erase(it);
  }

  size_type size() const {
    size_type count = 0;
    auto n = &mHead;
    while (n = n->mRhs[0]) count++;
    return count;
  }

  // no limitations as such
  size_type max_size() const { return std::numeric_limits<int>::max(); }

  bool empty() const { return nullptr == mHead.mRhs[0]; }

  compare_type key_comp() const { return mCompare; }

  allocator_type get_allocator() const { return allocator_type(); }

 private:
  // typedef for node alloator (as opposed to T allocator)
  typedef typename A::template rebind<node_type>::other node_allocator;

  // the supplied allocator
  node_allocator mNodeAllocator;

  // the supplied comparison object
  compare_type mCompare;

  // the initial node, which is a dummy entry in that it has no key
  node_type mHead;

  // static accessor to a useful buffer
  detail::pointers_to_node<A, node_type>& get_pointer_buffer() {
    static detail::pointers_to_node<A, node_type> instance;
    return instance;
  }

  // useful for inserts and deletes
  void discoverLinksToUpdate(const T& key, size_t height)
  {
    // n node is the last node prior to the insert
    auto n = &mHead;
    for (auto level = height; level; level--) {
      // walk as far to the right as we can
      while (n->mRhs[level - 1] && mCompare(n->mRhs[level - 1]->mKey, key))
        n = n->mRhs[level - 1];

      // record the location of the previous link
      get_pointer_buffer().mLhs[level - 1] = n;
    }
  }
};

template <class T>
void swap(skiplist<T>& lhs, skiplist<T>& rhs) {
  lhs.swap(rhs);
}

}

// TODO should iterator also be const.... i.e. is this basically a set not a list?
// TODO add comments - doxygen maybe?
// TODO initialisation list constructor
// TODO allow duplicates, optionally, as a template argument
// TODO profiler guided optimisation then branch prediction hints
// TODO pass in a custom allocator which is better?
// TODO profile (perf and gperf) then benchmark
// TODO reverse iterators
// TODO random access iterators
// TODO C++0x move constructors, move semantics on insert, etc
// TODO random access iterators
// TODO const iterators (and then update find/erase methods)
// TODO copy constructors etc
// TODO insert with range
// TODO emplace
// TODO equals, <, >, etc operators
// TODO other set methods such as upper/lower bound, etc
// TODO make fast as possible
// TODO consider conn access version/support?
// TODO unit tests for this - gtest, or just casserts?
// TODO other overloaded methods such as value based erasure, etc
// TODO can use allocator_traits in a later version of g++
// TODO support for front and back?
// TODO support mutable keys?
