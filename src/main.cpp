//
// Created by Malik T on 13/08/2025.
//

#include <iostream>
#include <print>
#include "core/OmegaException.hpp"
#include "core/test.hpp"

auto main() -> int
{
    try
    {
        test(1);
        test(123);
        test(-1);
    }
    catch (durak::core::OmegaException<bob> const& e)
    {
        std::print("{}",e);
    }
    catch (...){}
}