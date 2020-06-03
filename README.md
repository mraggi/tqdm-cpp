# Easy progress bars!

Create CLI progress bars easily!

# TQDM for C++17


This is a first attempt at implementing some of the functionality found in python's [tqdm](https://github.com/tqdm/tqdm) functionality.

**Note**: This project is not affiliated in any way with python's [tqdm](https://github.com/tqdm/tqdm). However, I plan on doing a pull request to [tqdm-cpp](https://github.com/tqdm/tqdm.cpp) which currently doesn't work (at least for me) after I've polished this enough.

# Installation

This is a header-only single-file, so installation is trivial: just copy the header `tqdm.hpp` to your include dirs.

# Basic Usage

The easiest way to see how this library is used is through an example:

```c++
#include <vector>
#include "tqdm.hpp"

int main()
{
    std::vector<int> A = {1,2,3,4,5,6};
    
    for (int a : tq::tqdm(A))
    {
        // do some heavy work, (e.g. sleep(1) to test, and #include <unistd.h>)
    }
    
    return 0;
}
```

This code displays a progress bar like this:
![progress bar](pbar.gif "progress bar")

It (should) work on any data structure. For convenience, as in tqdm, we include a trange function:

```c++
#include <vector>
#include "tqdm.hpp"

int main()
{
    for (int a : tq::trange(10))
    {
        // a will be in {0,1,...,9}
    }
    
    return 0;
}
```

The integer type you pass to `trange` determines the `value_type`. For example, `tq::trange(1000L)` contains `long`s.

# Prefixes and suffixes

It is easy to add extra info to the display.

```c++
    auto A = tq::tqdm(get_data_structure()); // works for rvalues too!
    A.set_prefix("Iterating over A: ");
    for (int a : A)
    {
        // do some calculations
        
        A << "loss = " << calculate_loss();
    }
```

Displays: ![progress bar](pbarwprefixsuffix.gif "progress bar")

# Notes

- By default, the progress bar is written to `std::cerr` so as to not clash with stdout redirectioning.
    - Modify this by calling `set_ostream` member function.
- Works with either rvalues or lvalues (with and without const). Takes ownership of rvalues (by moving).
- You can customize bar size by calling `set_bar_size`. Default is 30.
- By default, it only refreshes every 0.15 seconds (at most). Customize this with `set_min_update_time`
