#ifndef GREGJM_FLAT_SET_HPP
#define GREGJM_FLAT_SET_HPP

#include <algorithm> // std::sort, std::lower_bound, std::rotate,
                     // std::upper_bound
#include <functional> // std::less
#include <initializer_list> // std::initializer_list
#include <iterator> // std::iterator_traits
#include <memory> // std::allocator
#include <type_traits> // std::enable_if_t, std::is_convertible_v,
                       // std::is_nothrow_swappable_v, 
                       // std::is_nothrow_copy_constructible_v, std::true_type,
                       // std::false_type
#include <utility> // std::pair, std::declval, std::move
#include <vector> // std::vector

namespace gregjm {

template <typename Key, typename Compare = std::less<Key>,
          typename Allocator = std::allocator<Key>>
class FlatSet {
    using VectorT = std::vector<Key, Allocator>;
    using TraitsT = std::allocator_traits<Allocator>;

    static inline constexpr bool IS_NOTHROW_COMPARABLE =
        noexcept(std::declval<Compare>()(std::declval<const Key&>(),
                                         std::declval<const Key&>()));

public:
    using key_type = Key;
    using value_type = Key;
    using size_type = typename VectorT::size_type;
    using difference_type = typename VectorT::difference_type;
    using key_compare = Compare;
    using value_compare = Compare;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename TraitsT::pointer;
    using const_pointer = typename TraitsT::const_pointer;
    using iterator = typename VectorT::iterator;
    using const_iterator = typename VectorT::const_iterator;
    using reverse_iterator = typename VectorT::reverse_iterator;
    using const_reverse_iterator = typename VectorT::const_reverse_iterator;

    FlatSet() : FlatSet{ Compare{ } } { }

    explicit FlatSet(const Compare &comparator,
                     const Allocator &allocator = Allocator{ })
        : data_{ allocator }, comparator_{ comparator }
    { }

    explicit FlatSet(const Allocator &allocator) : data_{ allocator },
                                                   comparator_{ } { }

    template <typename Iter>
    FlatSet(const Iter first, const Iter last, const Compare &comparator,
            const Allocator &allocator = Allocator{ })
        : data_{ first, last, allocator }, comparator_{ comparator }
    {
        sort();
    }

    template <typename Iter>
    FlatSet(const Iter first, const Iter last, const Allocator &allocator)
        : FlatSet{ first, last, Compare{ }, allocator }
    { }

    FlatSet(const FlatSet &other) = default;

    FlatSet(const FlatSet &other, const Allocator &allocator)
        : data_{ other.data_, allocator }, comparator_{ other.comparator_ }
    { }

    FlatSet(FlatSet &&other) = default;

    FlatSet(FlatSet &&other, const Allocator &allocator)
        : data_{ std::move(other.data_), allocator },
          comparator_{ other.comparator_ }
    { }

    FlatSet(const std::initializer_list<value_type> init,
            const Compare &comparator = Compare{ },
            const Allocator &allocator = Allocator{ })
        : FlatSet{ init.begin(), init.end(), comparator, allocator }
    { }

    FlatSet(const std::initializer_list<value_type> init,
            const Allocator &allocator)
        : FlatSet{ init, Compare{ }, allocator }
    { }

    FlatSet& operator=(const FlatSet &other) = default;

    FlatSet& operator=(FlatSet &&other)
        noexcept(std::is_nothrow_move_assignable_v<VectorT>
                 and std::is_nothrow_move_assignable_v<Compare>)
        = default;

    FlatSet& operator=(const std::initializer_list<value_type> ilist) {
        data_ = ilist;
        sort();
    }

    allocator_type get_allocator() const
        noexcept(std::declval<const VectorT&>().get_allocator())
    {
        return data_.get_allocator();
    }

    iterator begin() noexcept {
        return data_.begin();
    }

    const_iterator begin() const noexcept {
        return data_.cbegin();
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    iterator end() noexcept {
        return data_.end();
    }

    const_iterator end() const noexcept {
        return data_.end();
    } 

    const_iterator cend() const noexcept {
        return end();
    }
    
    reverse_iterator rbegin() noexcept {
        return data_.rbegin();
    }

    const_reverse_iterator rbegin() const noexcept {
        return data_.crbegin();
    }

    const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }

    reverse_iterator rend() noexcept {
        return data_.rend();
    }

    const_reverse_iterator rend() const noexcept {
        return data_.rend();
    }

    const_reverse_iterator crend() const noexcept {
        return rend();
    }

    bool empty() const noexcept {
        return data_.empty();
    }

    size_type size() const noexcept {
        return data_.size();
    }

    size_type max_size() const noexcept {
        return data_.max_size();
    }

    void clear() noexcept {
        data_.clear();
    }

    std::pair<iterator, bool> insert(const value_type &value) {
        const auto found = lower_bound(value);

        if (found != end()) {
            return std::pair<iterator, bool>{ found, false };
        }

        return std::pair<iterator, bool>{ data_.insert(found, value), true };
    }

    std::pair<iterator, bool> insert(value_type &&value) {
        const auto found = lower_bound(value);

        if (found != end()) {
            return std::pair<iterator, bool>{ found, false };
        }

        return std::pair<iterator, bool>{ data_.insert(found, value), true };
    }

    iterator insert(const_iterator, const value_type &value) {
        return insert(value).first;
    }

    iterator insert(const_iterator, value_type &&value) {
        return insert(value).first;
    }

    template <typename Iter>
    void insert(Iter first, const Iter last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    void insert(const std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    template <typename ...Args>
    std::pair<iterator, bool> emplace(Args &&...args) {
        data_.emplace_back(std::forward<Args>(args)...);
        const auto &emplaced = data_.back();
        const auto emplaced_iter = data_.end() - 1;

        const auto found = std::lower_bound(begin(), end() - 1, emplaced,
                                            comparator_);

        if (found != end() - 1) {
            data_.pop_back();

            return std::pair<iterator, bool>{ found, false };
        }

        std::rotate(found, emplaced_iter, end());

        return std::pair<iterator, bool>{ found, true };
    }

    template <typename ...Args>
    std::pair<iterator, bool> emplace_hint(const_iterator, Args &&...args) {
        return emplace(std::forward<Args>(args)...);
    }

    iterator erase(const const_iterator iter) {
        return data_.erase(iter);
    }

    iterator erase(const const_iterator first, const const_iterator last) {
        return data_.erase(first, last);
    }

    size_type erase(const key_type &key) {
        const auto found = lower_bound(key);

        if (found != end()) {
            data_.erase(found);

            return 1;
        }

        return 0;
    }

    void swap(FlatSet &other) noexcept(std::is_nothrow_swappable_v<VectorT>) {
        using std::swap;

        swap(data_, other.data_);
    }

    size_type count(const value_type &value) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        if (lower_bound(value) == end()) {
            return 0;
        }

        return 1;
    }

    iterator find(const value_type &value) noexcept(IS_NOTHROW_COMPARABLE) {
        return lower_bound(value);
    }

    const_iterator find(const value_type &value) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lower_bound(value);
    }

    std::pair<iterator, iterator> equal_range(const value_type &value)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return std::equal_range(begin(), end(), value, comparator_);
    }

    std::pair<const_iterator, const_iterator>
    equal_range(const value_type &value) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return std::equal_range(cbegin(), cend(), value, comparator_);
    }

    iterator lower_bound(const value_type &value)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return std::lower_bound(begin(), end(), value, comparator_);
    }

    const_iterator lower_bound(const value_type &value) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return std::lower_bound(cbegin(), cend(), value, comparator_);
    }

    iterator upper_bound(const value_type &value)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return std::upper_bound(begin(), end(), value, comparator_);
    }

    const_iterator upper_bound(const value_type &value) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return std::upper_bound(cbegin(), cend(), value, comparator_);
    }

    key_compare key_comp() const
        noexcept(std::is_nothrow_copy_constructible_v<key_compare>)
    {
        return comparator_;
    }

    value_compare value_comp() const
        noexcept(std::is_nothrow_copy_constructible_v<value_compare>)
    {
        return comparator_;
    }

    friend inline bool operator==(const FlatSet &lhs, const FlatSet &rhs)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lhs.data_ == rhs.data_;
    }

    friend inline bool operator!=(const FlatSet &lhs, const FlatSet &rhs)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lhs.data_ != rhs.data_;
    }

    friend inline bool operator<(const FlatSet &lhs, const FlatSet &rhs)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lhs.data_ < rhs.data_;
    }

    friend inline bool operator<=(const FlatSet &lhs, const FlatSet &rhs)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lhs.data_ <= rhs.data_;
    }

    friend inline bool operator>(const FlatSet &lhs, const FlatSet &rhs)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lhs.data_ > rhs.data_;
    }

    friend inline bool operator>=(const FlatSet &lhs, const FlatSet &rhs)
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return lhs.data_ >= rhs.data_;
    }

private:
    inline bool eq(const value_type &lhs, const value_type &rhs) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return not comparator_(lhs, rhs) and not comparator_(rhs, lhs);
    }

    inline bool ne(const value_type &lhs, const value_type &rhs) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return comparator_(lhs, rhs) or comparator_(rhs, lhs);
    }

    inline bool lt(const value_type &lhs, const value_type &rhs) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return comparator_(lhs, rhs);
    }

    inline bool le(const value_type &lhs, const value_type &rhs) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return not comparator_(rhs, lhs);
    }

    inline bool gt(const value_type &lhs, const value_type &rhs) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return comparator_(rhs, lhs);
    }

    inline bool ge(const value_type &lhs, const value_type &rhs) const
        noexcept(IS_NOTHROW_COMPARABLE)
    {
        return not comparator_(lhs, rhs);
    }

    void sort() noexcept(IS_NOTHROW_COMPARABLE) {
        std::sort(begin(), end(), comparator_);
    }

    VectorT data_;
    value_compare comparator_;
};

template <typename Key, typename Compare, typename Allocator>
void swap(FlatSet<Key, Compare, Allocator> &lhs,
          FlatSet<Key, Compare, Allocator> &rhs)
    noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

}

#endif
