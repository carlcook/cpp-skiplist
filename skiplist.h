#pragma once

#include <stdlib.h>

#include <memory>
#include <vector>
#include <limits>

namespace nonstd {

namespace detail {

const size_t MAX_HEIGHT = 10;

// internal representation of a node within a skiplist
template <class Key, class Allocator>
struct node {
  const Key mKey;
  std::vector<node*, Allocator> mRhs;
  node(const Key& key, size_t height) : mKey(key), mRhs(height) {}
};

// static container to hold a temp buffer of pointers
template <class A, class Node>
struct pointers_to_node {
  typedef typename A::template rebind<Node*>::other allocator;
  Node** mLhs;

  pointers_to_node() 
      : srand(time(NULL)), mLhs(allocator().allocate(MAX_HEIGHT)) {}

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
  class iterator {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef typename allocator_type::value_type value_type;
    typedef typename allocator_type::const_reference const_reference;
    typedef typename allocator_type::const_pointer const_pointer;

    // standard says the iterator must be default constructable
    iterator() : mSkiplist(NULL), mNode(NULL) {}

    // copy constructor
    iterator(const iterator& other)
        : mSkiplist(other.mSkiplist), mNode(other.mNode) {}

    ~iterator() {
      mSkiplist = NULL;
      mNode = NULL;
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
    const skiplist* mSkiplist;
    node_type* mNode;

    // set pointer to head of list (excluding the default node)
    iterator(const skiplist* skiplist, node_type* node = NULL)
        : mSkiplist(skiplist), mNode(node) {}

   friend class skiplist;
  };

  skiplist() : mHead(T(), detail::MAX_HEIGHT) {}

  skiplist(const skiplist& other) : mHead(T(), detail::MAX_HEIGHT) {
    for (auto it = other.begin(); it != other.end(); ++it) insert(*it);
  }

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
    for (auto level = height; level; level--) {
      newnode->mRhs[level - 1] = get_pointer_buffer().mLhs[level - 1];
      get_pointer_buffer().mLhs[level - 1] = newnode;
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
    return iterator(this, NULL); // not found
  }

  iterator erase(const iterator position) {
    discoverLinksToUpdate(*position);

    for (auto level = detail::MAX_HEIGHT; level; level--) {
      if (!get_pointer_buffer().mPointers[level - 1]) continue;
      get_pointer_buffer().mLhs[level - 1] = position.mNode->mRhs[level - 1];
    }

    iterator next(this, position.mNode->mRhs[0]);
    mNodeAllocator.destroy(position.mNode);
    mNodeAllocator.deallocate(position.mNode, 1);
    return next;
  }

  iterator begin() const { return iterator(this, mHead.mRhs[0]); }

  iterator end() const { return iterator(this, NULL); }

  const_reference operator[](size_type position) const {
    auto n = mHead.mRhs[0];
    while (position-- > 0) n = n->mRhs[0];
    return n->mKey;
  }

  void clear() {
    auto it = begin();
    while ((it = erase(it)) != end());
  }

  size_type size() const {
    size_type count = 0;
    auto n = &mHead;
    while (n = n->mRhs[0]) count++;
    return count;
  }

  size_type max_size() const { return std::numeric_limits<int>::max(); }

  bool empty() const { return mHead.mRhs[0] == NULL; }

  compare_type key_comp() const { return mCompare; }

  allocator_type get_allocator() const { return allocator_type(); }

 private:
  // typedef for node alloator (as opposed to T allocator)
  typedef typename A::template rebind<node_type>::other node_allocator;

  node_allocator mNodeAllocator;
  compare_type mCompare;
  node_type mHead; // we never set or refer to the key

  // static accessor
  detail::pointers_to_node<A, node_type>& get_pointer_buffer() {
    static detail::pointers_to_node<A, node_type> instance;
    return instance;
  }

  void discoverLinksToUpdate(const T& key, size_t height = detail::MAX_HEIGHT)
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

template <class T, class Compare = std::less<T>, class A = std::allocator<T>>
void swap(skiplist<T,A>&, skiplist<T,A>&) {
  // TODO
}

}

// TODO clean up this code
// TODO initialisation list constructor
// TODO don't use a vector - use something better?
// TODO better randomisation (smarter - more balanced?), or even a rotating list?
// TODO allow duplicates, optionally, as a template argument
// TODO profiler guided optimisation then branch prediction hints
// TODO pass in a custom allocator which is better?
// TODO profile (perf and gperf) then benchmark
// TODO reverse iterators
// TODO random access iterators
// TODO C++0x move constructors, move semantics on insert, etc
// TODO random access iterators
// TODO const iterators (and then update find/erase methods)
// TODO swap
// TODO copy constructors etc
// TODO insert with range
// TODO emplace
// TODO equals, <, >, etc operators
// TODO other set methods such as upper/lower bound, etc
// TODO make fast as possible
// TODO consider conn access version/support?
// TODO unit tests for this
// TODO other overloaded methods such as value based erasure, etc
// TODO can use allocator_traits in a later version of g++
// TODO support for front and back?
// TODO support mutable keys?
