// Copyright (c) Microsoft Corporation. All rights reserved.

#include "loader.hpp"

int
main(int ac, char** av)
{
    return jbpf_lcm_cli::loader::run_loader(ac, av);
}