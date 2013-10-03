#include <stdio.h>

#include <iostream>
#include <iterator>
#include <vector>
#include <functional>

#include "skiplist.h"

// turn off debug statements
#define printf 

using namespace std;
using namespace nonstd;

class MyDataItem
{
  public:
    MyDataItem() {
      mData = new int[10];
      printf("Got created\n");
    }

    MyDataItem(MyDataItem&& other) {
      mData = other.mData;
      id = other.id;
      other.mData = NULL;
      printf("Got moved\n");
    }

    MyDataItem(const MyDataItem& other) {
      mData = new int(10);
      id = other.id;
      printf("Got copy constructed\n");
    }

    MyDataItem& operator=(const MyDataItem& other) {
      if (this != &other) {
        int* newData = new int[10];
        std::copy(other.mData, other.mData + 10, newData);

        delete [] mData;

        mData = newData;
        id = other.id;
        printf("Got assigned\n");
      }
      return *this;
    }

    MyDataItem& operator=(MyDataItem&& other) {
      if (this != &other) {
        delete [] mData;
        mData = other.mData;
        id = other.id;
        other.mData = NULL;
        printf("Got move assigned\n");
      }
      return *this;
    }

    ~MyDataItem() {
      delete mData;
      mData = NULL;
    }

    bool operator< (const MyDataItem& other) const {
      printf("comparing less %d and %d\n", id, other.id);
      return id < other.id;
    }

    bool operator> (const MyDataItem& other) const {
      printf("comparing greater %d and %d\n", id, other.id);
      return id > other.id;
    }

    int id;

  private:
 int* mData;
};

int main() {

  less<MyDataItem> myLess;
  skiplist<MyDataItem> myList(myLess);

  MyDataItem item1; item1.id = 1;
  MyDataItem item2; item2.id = 2;
  MyDataItem item3; item3.id = 3;
  myList.insert(item1);
  myList.insert(item3);
  myList.insert(item2);
  auto it = myList.find(item2);

  cout << (it == myList.end()) << endl;
  cout << it->id << endl;
  cout << (*it).id << endl;

  for (auto it2 = myList.begin(); it2 != myList.end(); ++it2)  
    cout << it2->id << endl;  

  cout << "next should be three after deleting two: " << myList.erase(it)->id << endl;

  for (auto it2 = myList.begin(); it2 != myList.end(); ++it2)  
    cout << it2->id << endl;  

  cout << "size is now: " << myList.size() << endl;

  cout << "last element is now: " << myList[myList.size() - 1].id << endl;

  auto allocator = myList.get_allocator();

  skiplist<MyDataItem> myList2(myList);

  cout << "last element in copy is now: " << myList2[myList2.size() - 1].id << endl;

  // greater
  greater<MyDataItem> myGreater;
  skiplist<MyDataItem, greater<MyDataItem>> myList3(myGreater);
  myList3.insert(item1);
  myList3.insert(item3);
  myList3.insert(item2);
  for (auto it = myList3.begin(); it != myList3.end(); ++it)
    cout << it->id << endl;


  MyDataItem item4; item4.id = 4;
  myList2.insert(item4);

  cout << "last element in copy is now: " << myList2[myList2.size() - 1].id << endl;

  cout << "first list now contains " << myList.size() << " elements" << endl;

  swap(myList, myList2);

  cout << "first list now contains " << myList.size() << " elements" << endl;
  
}
