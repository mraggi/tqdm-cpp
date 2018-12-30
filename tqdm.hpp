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
using index = std::ptrdiff_t; // maybe std::size_t, but I hate unsigned types.
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
class tqdm_for_lvalues;

template <class ForwardIter>
class iter_wrapper
{
public:
    using iterator_category = typename ForwardIter::iterator_category;
    using value_type = typename ForwardIter::value_type;
    using difference_type = typename ForwardIter::difference_type;
    using pointer = typename ForwardIter::pointer;
    using reference = typename ForwardIter::reference;
    using parent_t = tqdm_for_lvalues<ForwardIter>;

    iter_wrapper(ForwardIter it, parent_t* parent)
        : current_(it), parent_(parent)
    {}

    auto operator*() { return *current_; }

    void operator++() { ++current_; }

    bool operator!=(const iter_wrapper& other) const
    {
        parent_->update();
        return current_ != other.current_;
    }

    const ForwardIter& get() const { return current_; }

private:
    friend class tqdm_for_lvalues<ForwardIter>;
    ForwardIter current_;
    parent_t* parent_;
};

// -------------------- tqdm_for_lvalues --------------------

template <class ForwardIter>
class tqdm_for_lvalues
{
public:
    using iterator = iter_wrapper<ForwardIter>;
    using value_type = typename ForwardIter::value_type;
    using size_type = index;
    using difference_type = index;

    tqdm_for_lvalues(ForwardIter begin, ForwardIter end)
        : first_(begin, this)
        , last_(end, this)
        , num_iters_(std::distance(begin, end))
    {}

    tqdm_for_lvalues(ForwardIter begin, ForwardIter end, index total)
        : first_(begin, this), last_(end, this), num_iters_(total)
    {}

    template <class Container>
    explicit tqdm_for_lvalues(Container& C)
        : first_(C.begin(), this), last_(C.end(), this), num_iters_(C.size())
    {}

    template <class Container>
    explicit tqdm_for_lvalues(const Container& C)
        : first_(C.begin(), this), last_(C.end(), this), num_iters_(C.size())
    {}

    tqdm_for_lvalues(const tqdm_for_lvalues&) = delete;
    tqdm_for_lvalues(tqdm_for_lvalues&&) = delete;
    tqdm_for_lvalues& operator=(tqdm_for_lvalues&&) = delete;
    tqdm_for_lvalues& operator=(const tqdm_for_lvalues&) = delete;

    template <class Container>
    tqdm_for_lvalues(Container&&) = delete; // prevent misuse!

    iterator begin()
    {
        chronometer_.reset();
        refresh_.reset();
        iters_done_ = 0;
        return first_;
    }

    iterator end() const { return last_; }

    void update()
    {
        if (time_since_refresh() > min_time_per_update_ || iters_done_ == 0 ||
            iters_left() == 0)
        {
            reset_refresh_timer();
            print_progress();
        }

        ++iters_done_;
        suffix_.str("");
    }

    void set_ostream(std::ostream& os) { os_ = &os; }
    void set_prefix(std::string s) { prefix_ = std::move(s); }
    void set_bar_size(int size) { bar_size_ = size; }
    void set_min_update_time(double time) { min_time_per_update_ = time; }

    template <class T>
    tqdm_for_lvalues& operator<<(const T& t)
    {
        suffix_ << t;
        return *this;
    }

    void manually_set_advancement(double to)
    {
        if (to > 1.)
            to = 1.;
        if (to < 0.)
            to = 0.;
        iters_done_ = std::round(to*num_iters_);
    }

private:
    index iters_left() const { return num_iters_ - iters_done_; }

    void print_progress()
    {
        auto flags = os_->flags();

        double complete = calc_advancement();
        double t = chronometer_.peek();
        double eta = t/complete - t;

        std::stringstream bar;

        bar << '\r' << prefix_ << '{' << std::fixed << std::setprecision(1)
            << std::setw(4) << 100*complete << "%} ";

        print_bar(bar, complete);

        bar << " (" << std::setw(4) << t << "s < " << eta << "s) ";

        std::string sbar = bar.str();
        std::string suffix = suffix_.str();

        index out_size = sbar.size() + suffix.size();
        term_cols_ = std::max(term_cols_, out_size);
        index num_blank = term_cols_ - out_size;

        (*os_) << sbar << suffix << std::string(num_blank, ' ') << std::flush;

        os_->flags(flags);
    }

    double calc_advancement() const
    {
        return iters_done_/(num_iters_ + 0.0000000000001);
    }

    void print_bar(std::stringstream& ss, double filled) const
    {
        auto num_filled = static_cast<index>(std::round(filled*bar_size_));
        ss << '[' << std::string(num_filled, '#')
           << std::string(bar_size_ - num_filled, ' ') << ']';
    }

    double time_since_refresh() const { return refresh_.peek(); }
    void reset_refresh_timer() { refresh_.reset(); }

    iterator first_;
    iterator last_;
    index num_iters_;
    index iters_done_{0};

    Chronometer chronometer_{};
    Chronometer refresh_{};
    double min_time_per_update_{0.15}; // found experimentally

    std::ostream* os_{&std::cerr};

    index bar_size_{30};
    index term_cols_{1};

    std::string prefix_{};
    std::stringstream suffix_{};
};

template <class Container>
tqdm_for_lvalues(Container&)->tqdm_for_lvalues<typename Container::iterator>;

template <class Container>
tqdm_for_lvalues(const Container&)
  ->tqdm_for_lvalues<typename Container::const_iterator>;

// -------------------- tqdm_for_rvalues --------------------

template <class Container>
class tqdm_for_rvalues
{
public:
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using value_type = typename Container::value_type;

    explicit tqdm_for_rvalues(Container&& C)
        : C_(std::forward<Container>(C)), tqdm_(C_)
    {}

    auto begin() { return tqdm_.begin(); }

    auto end() { return tqdm_.end(); }

    void update() { return tqdm_.update(); }

    void set_ostream(std::ostream& os) { tqdm_.set_ostream(os); }
    void set_prefix(std::string s) { tqdm_.set_prefix(std::move(s)); }
    void set_bar_size(int size) { tqdm_.set_bar_size(size); }
    void set_min_update_time(double time) { tqdm_.set_min_update_time(time); }

    template <class T>
    auto& operator<<(const T& t)
    {
        return tqdm_ << t;
    }

    void advance(index amount) { tqdm_.advance(amount); }

    void manually_set_advancement(double to)
    {
        tqdm_.manually_set_advancement(to);
    }

private:
    Container C_;
    tqdm_for_lvalues<iterator> tqdm_;
};

template <class Container>
tqdm_for_rvalues(Container &&)->tqdm_for_rvalues<Container>;

// -------------------- tqdm --------------------
template <class ForwardIter>
auto tqdm(const ForwardIter& first, const ForwardIter& last)
{
    return tqdm_for_lvalues(first, last);
}

template <class ForwardIter>
auto tqdm(const ForwardIter& first, const ForwardIter& last, index total)
{
    return tqdm_for_lvalues(first, last, total);
}

template <class Container>
auto tqdm(const Container& C)
{
    return tqdm_for_lvalues(C);
}

template <class Container>
auto tqdm(Container& C)
{
    return tqdm_for_lvalues(C);
}

template <class Container>
auto tqdm(Container&& C)
{
    return tqdm_for_rvalues(std::forward<Container>(C));
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

    explicit int_iterator(IntType val) : value_(val) {}

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

    difference_type operator-(const int_iterator& other) const
    {
        return value_ - other.value_;
    }

    bool operator!=(const int_iterator& other) const
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
    explicit range(IntType last) : first_(0), last_(last) {}

    iterator begin() const { return first_; }
    iterator end() const { return last_; }
    index size() const { return last_ - first_; }

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
