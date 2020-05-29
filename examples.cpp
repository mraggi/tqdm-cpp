#include <algorithm>
#include <numeric>
#include <unistd.h>
#include <vector>

#include "tqdm.hpp"

const int sleep_time = 200;

std::vector<int> get_vector(int size)
{
    std::vector<int> A(size);
    std::iota(A.begin(), A.end(), 1000);
//     std::generate(A.begin(), A.end(), []() {return rand(); } );

    return A;
}

void test_rvalue()
{
    auto T = tq::tqdm(get_vector(5000));
    T.set_prefix("tqdm from rvalue ");
    for (auto t : T)
    {
        usleep(sleep_time);
        T << t;
    }
}

void test_lvalue()
{
    auto A = get_vector(5000);
    auto T = tq::tqdm(A);
    T.set_prefix("tqdm from lvalue ");
    for (auto&& t : T)
    {
        t *= 2;
        usleep(sleep_time);
        T << t;
    }
}

void test_constlvalue()
{
    const std::vector<int> A = get_vector(5000);
    auto T = tq::tqdm(A);
    T.set_prefix("tqdm from const lvalue ");
    for (auto&& t : T)
    {
        usleep(sleep_time);
        T << t;
    }
}

void test_trange()
{
    auto T = tq::trange(100, 5000);
    T.set_prefix("tqdm range ");
    for (auto t : T)
    {
        usleep(sleep_time);
        T << t;
    }
}

void test_timer()
{
    tq::tqdm_timer timer(2.0);
    timer.set_prefix("tqdm timer ");
    for (auto a : timer)
    {
        usleep(30000);
    }
}

int main()
{
    test_timer();
    std::cout << '\n';
    test_lvalue();
    std::cout << '\n';
    test_constlvalue();
    std::cout << '\n';
    test_rvalue();
    std::cout << '\n';
    test_trange();
    std::cout << '\n';

    return 0;
}
