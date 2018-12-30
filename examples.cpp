#include <numeric>
#include <unistd.h>
#include <vector>

#include "tqdm.hpp"

using tq::tqdm;
using tq::trange;

std::vector<int> get_vector(int size)
{
    std::vector<int> A(size);
    std::iota(A.begin(), A.end(), 1000);
    return A;
}

void test_rvalue()
{
    auto T = tqdm(get_vector(5000));
    T.set_prefix("tqdm from rvalue");
    for (auto t : T)
    {
        usleep(200);
        T << t;
    }
}

void test_lvalue()
{
    auto A = get_vector(5000);
    auto T = tqdm(A);
    T.set_prefix("tqdm from lvalue ");
    for (auto&& t : T)
    {
        t *= 2;
        usleep(200);
        T << t;
    }
}

void test_constlvalue()
{
    const std::vector<int> A = get_vector(5000);
    auto T = tqdm(A);
    T.set_prefix("tqdm from const lvalue ");
    for (auto&& t : T)
    {
        usleep(200);
        T << t;
    }
}

void test_trange()
{
    auto T = trange(100, 5000);
    T.set_prefix("tqdm range ");
    for (auto t : T)
    {
        usleep(200);
        T << t;
    }
}


int main()
{

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
