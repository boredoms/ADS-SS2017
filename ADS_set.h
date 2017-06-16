#ifndef ADS_SET_H
#define ADS_SET_H

#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>

template <typename Key, size_t N = 25/* implementation-defined */>
class ADS_set {
public:
    class Iterator;
    using value_type = Key;
    using key_type = Key;
    using reference = key_type&;
    using const_reference = const key_type&;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using key_compare = std::less<key_type>;   // B+-Tree
    using key_equal = std::equal_to<key_type>; // Hashing
    using hasher = std::hash<key_type>;        // Hashing

private:
    static const size_type order = 2 * N;

    struct Node;

    struct Node {
        size_type node_size;
        bool leaf;
        key_type *data;
        Node **children;
        Node *next;

        Node() {
            data = new key_type[order + 1];
            node_size = 0;
            next = nullptr;
        }

        Node(bool is_leaf, Node *next_node = nullptr) : Node() {
            leaf = is_leaf;
            next = next_node;
        }

        ~Node() {
            if (!leaf) {
                for (size_type i = 0; i <= node_size; ++i) {
                    delete children[i];
                }
                delete[] children;
            }
            delete[] data;
        }

        key_type &minimum() { // recursive minimum search from this node
            if (leaf) {
                return data[0];
            } else {
                return children[0]->minimum();
            }
        }

        Node *insert_leaf(const key_type &key, size_type position) {
            for (size_type i = node_size; i > position; --i) {
                data[i] = data[i - 1];
            }

            data[position] = key;
            node_size++;

            if (node_size > order) {
                return split();
            } else {
                return nullptr;
            }
        }

        Node *insert_node(const key_type &key, size_type position) {
            Node *ret = children[position]->insert(key);

            if (ret == nullptr) {
                return nullptr;
            }

            for (size_type i = node_size; i > position; --i) {
                data[i] = data[i - 1];
                children[i + 1] = children[i];
            }
            data[position] = ret->minimum();
            children[position + 1] = ret;

            node_size++;

            if (node_size > order) {
                return split();
            } else {
                return nullptr;
            }
        }

        Node *insert(const key_type &key) {
            size_type i = 0;

            for (; i < node_size; ++i) {
                if (std::equal_to<key_type>()(key, data[i])) {
                    return nullptr;
                }
                if (std::less<key_type>()(key, data[i])) {
                    break;
                }
            }

            if (leaf) {
                return insert_leaf(key, i);
            } else {
                return insert_node(key, i);
            }
        }

        Node *split() {
            size_type half = order / 2;

            Node *right = new Node(leaf, next);
            if (!leaf) {
                right->children = new Node*[order + 2];
            }
            for (size_type i = 0; i < half; ++i) {
                right->data[i] = data[half + 1 + i];
                if (!leaf) {
                    right->children[i + 1] = children[half + 2 + i];
                }
            }
            if (!leaf) {
                right->children[0] = children[half + 1];
                node_size = half;
            } else  {
                node_size = half + 1;
            }

            right->node_size = half;
            next = right;

            return right;
        }

        iterator find(const key_type &key) {
            size_type i = 0;
            for (; i < node_size; ++i) {
                if (std::equal_to<key_type>()(key, data[i]) && leaf) {
                    return Iterator(this, i);
                }
                if (std::less<key_type>()(key, data[i])) {
                    break;
                }
            }

            if (leaf) {
                return Iterator();
            } else {
                return children[i]->find(key);
            }
        }

        void move_into_left(Node *left, Node *right) {
            // borrows one element from the right node into the left node
            if (left->leaf) {
                left->data[left->node_size] = right->data[0];
                left->node_size++;

                for (size_type i = 0; i < right->node_size - 1; ++i) {
                    right->data[i] = right->data[i + 1];
                }
                right->node_size--;
            } else {
                // move first child into left node
                left->data[left->node_size++] = right->children[0]->minimum();
                left->children[left->node_size] = right->children[0];
                // fix right node
                for (size_type i = 0; i < right->node_size - 1; ++i) {
                    right->data[i] = right->data[i + 1];
                    right->children[i] = right->children[i + 1];
                }
                right->children[right->node_size - 1] = right->children[right->node_size];
                right->node_size--;
            }
        }

        void move_into_right(Node *left, Node *right) {
            // borrows one element from the left element and moves it into right one
            if (left->leaf) {
                for (size_type i = right->node_size; i > 0; --i) {
                    right->data[i] = right->data[i - 1];
                }

                right->data[0] = left->data[left->node_size - 1];
                right->node_size++;
                left->node_size--;
            } else {
                for (size_type i = right->node_size; i > 0; --i) {
                    right->data[i] = right->data[i - 1];
                    right->children[i + 1] = right->children[i];
                }
                right->children[1] = right->children[0];
                right->data[0] = right->children[1]->minimum();
                right->children[0] = left->children[left->node_size];
                
                right->node_size++;
                left->node_size--;
            }
        }


        Node *merge_and_split(Node *left, Node *right) {
            if (left->node_size < order / 2) {
                move_into_left(left, right);
                return right;
            } else {
                move_into_right(left, right);
                return left;
            }
        }

        void merge_delete() {
            if (!leaf) {
                delete[] children;
                leaf = true;
            }
            delete this;
        }

        Node *merge(Node *left, Node *right) {
            // merges two nodes and then deletes right one
            left->next = right->next;
            left->data[left->node_size] = right->minimum();
            if (!left->leaf) {
                left->children[++left->node_size] = right->children[0];
            }
            for (size_type i = 0; i < right->node_size; ++i) {
                left->data[left->node_size + i] = right->data[i];
                if (!left->leaf) {
                    left->children[left->node_size + 1 + i] = right->children[i + 1];
                }
            }

            left->node_size = left->node_size + right->node_size;
            right->merge_delete();
            return left;
        }


        Node *erase_node(const key_type& key, size_type position) {
            Node *ret = children[position]->erase(key);

            if (ret == nullptr) {
                return nullptr;
            } else {
                size_type key_number = 0;
                for (; key_number <= node_size; ++key_number) {
                    if (ret == children[key_number]) {
                        break;
                    }
                }

                if (ret->node_size >= order / 2) {
                    if (key_number == 0) {
                        return this;
                    } else {
                        data[key_number - 1] = ret->minimum();
                        return nullptr;
                    }
                } else {
                    Node *mergee;
                    if (key_number == node_size) {
                        mergee = children[node_size - 1];
                    } else {
                        mergee = children[key_number + 1];
                    }

                    if (mergee->node_size + ret->node_size < order) {
                        // merge children
                        if (key_number == node_size) {
                            merge(children[node_size - 1], ret);
                            // in this case we can just make the node smaller, no keys need fixing
                            node_size--;
                            if (node_size >= order / 2) {
                                return nullptr;
                            } else {
                                return this;
                            }
                        } else {
                            merge(ret, children[key_number + 1]);
                            // repair the keys
                            for (size_type i = key_number + 1; i < node_size; ++i) {
                                data[i - 1] = data[i];
                                children[i] = children[i + 1];
                            }
                            node_size--;

                            if (key_number > 0) {
                                data[key_number - 1] = ret->minimum();
                            }
                            if (key_number == 0 || node_size < order / 2) {
                                return this;
                            } else {
                                return nullptr;
                            }
                        }
                    } else {
                        // merge and split
                        if (key_number == node_size) {
                            merge_and_split(mergee, ret);
                            data[node_size - 1] = ret->minimum();
                            return nullptr;
                        } else {
                            merge_and_split(ret, mergee);
                            if (key_number == 0) {
                                data[0] = children[1]->minimum();
                                return this;
                            } else {
                                data[key_number - 1] = children[key_number]->minimum();
                                data[key_number] = children[key_number + 1]->minimum();
                                return nullptr;
                            }
                        }
                    }
                }
            }
        }

        Node *erase_leaf(size_type position) {
            for (size_type i = position; i < node_size - 1; ++i) {
                data[i] = data[i + 1];
            }
            node_size--;
            return this;
        }

        Node *erase(const key_type& key) {
            size_type i = 0;

            for (; i < node_size; ++i) {
                if ((std::equal_to<key_type>()(key, data[i]) && leaf) || std::less<key_type>()(key, data[i])) {
                    break;
                }
            }

            if (leaf) {
                return erase_leaf(i);
            } else {
                return erase_node(key, i);
            }
        }

        std::ostream& print(std::ostream& o) {
            o << "Node at [" << this << "] size=" << node_size << ": ";
            o << (leaf ? "(leaf)" : "(node)") << " [";
            for (size_type i = 0; i < node_size; ++i) {
                o << data[i] << (i < node_size - 1 ? ", " : "");
            }
            o << "]";
            if (leaf) {
                o << " next: " << next;
            }
            o << std::endl;

            return o;
        }

        std::ostream& rec_print(std::ostream& o) {
            print(o);
            if (!leaf) {
                for (size_type i = 0; i <= node_size; ++i) {
                    children[i]->rec_print(o);
                }
            }
            return o;
        }

        friend std::ostream& operator<<(std::ostream& o, Node &node) {
            return node.print(o);
        }
    };

    std::ostream& print(std::ostream& o) const {
        o << "tree size is " << leaves << std::endl;
        o << "head is: " << head << std::endl;
        root->rec_print(o);
        return o;
    }

    size_type leaves;// to ensure O(1) for size()
    Node *root;
    Node *head; // points to the first leaf, used for iteration

public:

    ADS_set() {
        // creates an empty leaf as the root
        root = new Node(true);
        head = root;
        leaves = 0;
    }

    ADS_set(std::initializer_list<key_type> ilist) : ADS_set() {
        // creates an empty tree and then inserts elements from ilist
        for (key_type i : ilist) {
            insert(i);
        }
    }

    template<typename InputIt> ADS_set(InputIt first, InputIt last) : ADS_set() {
        // inserts the elements in the range from first to last
        while(first != last) {
            insert(*first);
            ++first;
        }
    }

    ADS_set(const ADS_set& other) : ADS_set() {
        for (key_type i : other) {
            insert(i);
        }
    }

    ~ADS_set() {
        delete root;
    }

    ADS_set& operator=(const ADS_set& other) {
        if (this == &other) {
            return *this;
        }

        clear();

        for (key_type i : other) {
            insert(i);
        }

        return *this;
    }

    ADS_set& operator=(std::initializer_list<key_type> ilist) {
        // first clears out the tree, then inserts elements in ilist
        clear();

        for (key_type i : ilist) {
            insert(i);
        }
        return *this;
    }

    size_type size() const {
        // returns the number of leaves in the tree
        return leaves;
    }

    bool empty() const {
        // returns a boolean denoting if the tree is empty or not
        if (leaves == 0) {
            return true;
        } else {
            return false;
        }
    }

    size_type count(const key_type& key) const {
        // returns 0 if the element is not found, 1 otherwise (set is associative)
        if (find(key) == end()) {
            return 0;
        } else {
            return 1;
        }
    }

    iterator find(const key_type& key) const {
        if (empty()) {
            return end();
        } else {
            return root->find(key);
        }
    }

    void clear() {
        delete root;

        root = new Node(true);
        head  = root;
        leaves = 0;
    }
    void swap(ADS_set& other) {
        // swaps all instance variables
        Node *temp_ptr = other.root;
        size_type temp_sz = other.leaves;
        other.root = root;
        root = temp_ptr;

        temp_ptr = other.head;
        other.head = head;
        head = temp_ptr;

        other.leaves = leaves;
        leaves = temp_sz;
    }

    void insert(std::initializer_list<key_type> ilist) {
        for (key_type i : ilist) {
            insert(i);
        }
    };


    std::pair<iterator,bool> insert(const key_type& key) {
        iterator i = find(key);
        if(i != end()) {
            return std::pair<iterator, bool>(i, false);
        } else {
            Node *ret = root->insert(key);

            if (ret != nullptr) {
                Node *new_root = new Node;
                new_root->leaf = false;
                new_root->children = new Node*[order + 2];

                new_root->children[0] = root;
                new_root->children[1] = ret;

                new_root->data[0] = ret->minimum();
                new_root->node_size = 1;
                root = new_root;
            }
            leaves++;
            i = find(key);
            return std::pair<iterator, bool>(i, true);
        }
    }

    template<typename InputIt> void insert(InputIt first, InputIt last) {
        while (first != last) {
            insert(*first);
            ++first;
        }
    }

    size_type erase(const key_type& key) {
        if (find(key) == end()) {
            return 0;
        } else {
            root->erase(key);
            if (root->node_size == 0 && !root->leaf) {
                Node *temp = root;
                root = root->children[0];
                temp->merge_delete();
                //delete[] temp->children;
                //temp->leaf = true;
                //delete temp;
            }

            leaves--;
            return 1;
        }
    }

    const_iterator begin() const {
        if (empty()) {
            return end();
        } else {
            return Iterator(head, 0);
        }
    }
    const_iterator end() const {
        return Iterator();
    }

    void dump(std::ostream& o = std::cerr) const {
        print(o);
    }

    friend bool operator==(const ADS_set& lhs, const ADS_set& rhs) {
        if (rhs.leaves != lhs.leaves) {
            return false;
        } else {
            if (rhs.leaves == 0) {
                return true;
            }

            iterator lhs_iterator{lhs.begin()};
            iterator rhs_iterator{rhs.begin()};

            while (lhs_iterator != lhs.end()) {
                    if (std::equal_to<typename Iterator::value_type>{}(*lhs_iterator, *rhs_iterator)) {
                        ++lhs_iterator;
                        ++rhs_iterator;
                    } else {
                        return false;
                    }
                }
            return true;
        }
    }
    friend bool operator!=(const ADS_set& lhs, const ADS_set& rhs) { return !(lhs == rhs); }
};

template <typename Key, size_t N>
class ADS_set<Key,N>::Iterator {
public:
    using value_type = Key;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;
    using iterator_category = std::forward_iterator_tag;
private:
    Node *leaf;
    size_t position;
public:

    explicit Iterator(Node *node = nullptr, size_t offset = 0) : leaf{node}, position{offset} {}

    reference operator*() const {
        return leaf->data[position];
    }

    pointer operator->() const {
        return (leaf->data + position);
    }

    Iterator& operator++() {
        position++;
        if (position >= leaf->node_size) {
            leaf = leaf->next;
            position = 0;
        }
        return *this;
    }

    Iterator operator++(int) {
        iterator temp{*this};
        ++*this;
        return temp;
    }

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
        if (rhs.leaf == lhs.leaf && lhs.position == rhs.position) {
            return true;
        } else {
            return false;
        }
    }

    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
        return !(lhs == rhs);
    }

    bool refers_nullptr() { return (leaf == nullptr); } // helper function

    std::ostream &print(std::ostream &o) {
        o << "Iterator (leaf=" << leaf << ", offset=" << position << ")\n";
        return o;
    }
};

template <typename Key, size_t N> void swap(ADS_set<Key,N>& lhs, ADS_set<Key,N>& rhs) { lhs.swap(rhs); }

#endif // ADS_SET_H
