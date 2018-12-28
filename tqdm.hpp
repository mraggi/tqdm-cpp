#pragma once

/*
*Copyright (c) 2018-2019 <Miguel Raggi> <mraggi@gmail.com>
 *
*Permission is hereby granted, free of charge, to any person
*obtaining a copy of this software and associated documentation
*files (the "Software"), to deal in the Software without
*restriction, including without limitation the rights to use,
*copy, modify, merge, publish, distribute, sublicense, and/or sell
*copies of the Software, and to permit persons to whom the
*Software is furnished to do so, subject to the following
*conditions:
 *
*The above copyright notice and this permission notice shall be
*included in all copies or substantial portions of the Software.
 *
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
*EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
*OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
*NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
*HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
*OTHER DEALINGS IN THE SOFTWARE.
 */

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <type_traits>

// -------------------- chrono stuff --------------------

namespace tq
{
using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

inline double elapsed_seconds(time_point_t from, time_point_t to)
{
    using seconds = std::chrono::duration<double>;
    return std::chrono::duration_cast<seconds>(to - from).count();
}

class Chronometer
{
public:
    Chronometer() : start_(std::chrono::steady_clock::now()) {}

    double reset()
    {
        auto previous = start_;
        start_ = std::chrono::steady_clock::now();

        return elapsed_seconds(previous, start_);
    }

    double peek() const
    {
        auto now = std::chrono::steady_clock::now();

        return elapsed_seconds(start_, now);
    }

    time_point_t start_;
};

// -------------------- iter_wrapper --------------------

template <class ForwardIter>
class TqdmForLvalues;

template <class ForwardIter>
class iter_wrapper
{
public:
    using iterator_category = typename ForwardIter::iterator_category;
    using value_type = typename ForwardIter::value_type;
    using difference_type = typename ForwardIter::difference_type;
    using pointer = typename ForwardIter::pointer;
    using reference = typename ForwardIter::reference;

    iter_wrapper(ForwardIter it, TqdmForLvalues<ForwardIter>* parent)
        : current_(it), parent_(parent)
    {}

    auto operator*() { return *current_; }

    void operator++() { ++current_; }

    bool operator!=(const iter_wrapper& other)
    {
        parent_->update();
        return current_ != other.current_;
    }

    const ForwardIter& get() const { return current_; }

private:
    friend class TqdmForLvalues<ForwardIter>;
    ForwardIter current_;
    TqdmForLvalues<ForwardIter>* parent_;
};

// -------------------- TqdmForLvalues --------------------

template <class ForwardIter>
class TqdmForLvalues
{
public:
    using iterator = iter_wrapper<ForwardIter>;
    TqdmForLvalues(ForwardIter begin, ForwardIter end)
        : first_(begin, this), last_(end, this)
    {}
    TqdmForLvalues(const TqdmForLvalues&) = delete;
    TqdmForLvalues(TqdmForLvalues&&) = delete;

    template <class Container>
    TqdmForLvalues(Container& C) : first_(C.begin(), this), last_(C.end(), this)
    {}

    template <class Container>
    TqdmForLvalues(const Container& C)
        : first_(C.begin(), this), last_(C.end(), this)
    {}

    template <class Container>
    TqdmForLvalues(Container&&) = delete; // prevent misuse!

    iterator begin()
    {
        chronometer_.reset();
        clear_chrono_.reset();
        iters_left_ = std::distance(first_.current_, last_.current_);
        return first_;
    }

    iterator end() { return last_; }

    void update()
    {
        if (time_since_last_clear() > min_time_per_update_ ||
            iters_done_ == 0 || iters_left_ == 0)
        {
            clear_chrono_.reset();
            clear_line();
            print_progress();
        }

        ++iters_done_;
        --iters_left_;
        suffix_.clear();
    }

    void set_ostream(std::ostream& os) { os_ = &os; }
    void set_prefix(std::string s) { prefix_ = std::move(s); }

    template <class T>
    TqdmForLvalues& operator<<(const T& t)
    {
        std::stringstream ss;
        ss << t;
        suffix_ += ss.str();
        return *this;
    }

    void set_bar_size(int size) { bar_size_ = size; }
    void set_min_update_time(double time) { min_time_per_update_ = time; }

private:
    void clear_line()
    {
        (*os_) << '\r';
        for (int i = 0; i < 80; ++i)
            (*os_) << ' ';
        (*os_) << '\r';
    }

    void print_progress()
    {
        auto flags = os_->flags();
        double total = iters_done_ + iters_left_ + 0.0000000000001;

        double complete = double(iters_done_)/total;
        double t = chronometer_.peek();
        double eta = t/complete - t;

        (*os_) << prefix_ << '{' << std::fixed << std::setprecision(1)
               << std::setw(4) << 100*complete << "%} ";

        print_bar(complete, bar_size_);

        (*os_) << " (" << std::setw(4) << t << "s < " << eta << "s) ";

        os_->flags(flags);

        (*os_) << suffix_ << std::flush;
    }

    void print_bar(double filled, std::int64_t size) const
    {
        std::int64_t num_filled = std::round(filled*size);
        (*os_) << '[';
        for (int i = 0; i < num_filled; ++i)
            (*os_) << '#';
        for (int i = num_filled; i < size; ++i)
            (*os_) << ' ';
        (*os_) << ']';
    }

    double time_since_last_clear() const { return clear_chrono_.peek(); }

    iterator first_;
    iterator last_;

    Chronometer chronometer_{};
    Chronometer clear_chrono_{};
    std::ostream* os_{&std::cerr};
    double min_time_per_update_{0.15}; // found experimentally
    std::string prefix_{};
    std::string suffix_{};
    std::int64_t iters_done_{0};
    std::int64_t iters_left_{0};
    std::int64_t bar_size_{30};
};

template <class Container>
TqdmForLvalues(Container&)->TqdmForLvalues<typename Container::iterator>;

template <class Container>
TqdmForLvalues(const Container&)->TqdmForLvalues<typename Container::const_iterator>;

// -------------------- TqdmForRvalues --------------------

template <class Container>
class TqdmForRvalues
{
public:
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using value_type = typename Container::value_type;

    TqdmForRvalues(Container&& C)
        : C_(std::forward<Container>(C)), Tqdm_(C_.begin(), C_.end())
    {}

    auto begin() { return Tqdm_.begin(); }

    auto end() { return Tqdm_.end(); }

    void update() { return Tqdm_.update(); }

    void set_ostream(std::ostream& os) { Tqdm_.set_ostream(); }
    void set_prefix(std::string s) { Tqdm_.set_prefix(s); }

    template <class T>
    auto& operator<<(const T& t)
    {
        return Tqdm_ << t;
    }

    void set_bar_size(int size) { Tqdm_.set_bar_size(size); }

private:
    Container C_;
    TqdmForLvalues<iterator> Tqdm_;
};

template <class Container>
TqdmForRvalues(Container &&)->TqdmForRvalues<Container>;

// -------------------- tqdm --------------------
template <class ForwardIter>
auto tqdm(const ForwardIter& first, const ForwardIter& last)
{
    return TqdmForLvalues(first, last);
}

template <class Container>
auto tqdm(const Container& C)
{
    return TqdmForLvalues(C);
}

template <class Container>
auto tqdm(Container& C)
{
    return TqdmForLvalues(C);
}

template <class Container>
auto tqdm(Container&& C)
{
    return TqdmForRvalues(std::forward<Container>(C));
}

// -------------------- int_iterator --------------------

template <class IntType>
class int_iterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = IntType;
    using difference_type = IntType;
    using pointer = IntType*;
    using reference = IntType&;

    int_iterator(IntType val) : value_(val) {}

    IntType& operator*() { return value_; }

    int_iterator& operator++()
    {
        ++value_;
        return *this;
    }
    int_iterator& operator--()
    {
        --value_;
        return *this;
    }
    int_iterator& operator+=(difference_type d)
    {
        value_ += d;
        return *this;
    }

    difference_type operator-(const int_iterator& other)
    {
        return value_ - other.value_;
    }

    bool operator!=(const int_iterator& other)
    {
        return value_ != other.value_;
    }

private:
    IntType value_;
};

// -------------------- range --------------------
template <class IntType>
class range
{
public:
    using iterator = int_iterator<IntType>;
    using const_iterator = iterator;
    using value_type = IntType;

    range(IntType first, IntType last) : first_(first), last_(last) {}
    range(IntType last) : first_(0), last_(last) {}

    iterator begin() const { return first_; }
    iterator end() const { return last_; }

private:
    iterator first_;
    iterator last_;
};

template <class IntType>
auto trange(IntType first, IntType last)
{
    return tqdm(range(first, last));
}

template <class IntType>
auto trange(IntType last)
{
    return tqdm(range(last));
}
} // namespace tq
