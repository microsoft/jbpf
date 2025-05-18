// Copyright (c) Microsoft Corporation. All rights reserved.

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "yaml-cpp/yaml.h"

#include "parser.hpp"
#include "loader.hpp"

#include "getopt.h"
#include "jbpf_common.h"
#include "jbpf_lcm_api.h"
#include "jbpf_lcm_ipc.h"

namespace jbpf_lcm_cli {
namespace loader {

using namespace std;

enum lcm_cli_type
{
    load,
    unload
};

union request
{
    jbpf_codeletset_load_req load;
    jbpf_codeletset_unload_req unload;
};

struct lcm_cli_config
{
    request req;
    lcm_cli_type typ;
    jbpf_lcm_ipc_address_t addr;
};

inline bool
exists(const string& name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

load_req_outcome
parseArgs(int ac, char** av, lcm_cli_config* opts)
{
    stringstream default_address;
    default_address << JBPF_DEFAULT_RUN_PATH << "/" << JBPF_DEFAULT_NAMESPACE << "/" << JBPF_DEFAULT_LCM_SOCKET;
    auto address = default_address.str();
    string filename;
    bool set_direction = false;
    bool set_config = false;

    for (;;) {
        switch (getopt(ac, av, "a:c:lu")) {
        case 'a': {
            address = string(optarg);
            if (address.length() > JBPF_LCM_IPC_ADDRESS_LEN - 1) {
                cout << "-a length must be at most " << JBPF_LCM_IPC_ADDRESS_LEN - 1 << endl;
                return JBPF_LCM_CLI_INVALID_ARGS;
            }
            continue;
        }

        case 'c': {
            filename = string(optarg);
            continue;
        }

        case 'l': {
            if (set_direction) {
                cout << "Invalid arguments: cannot specify both load and unload" << endl;
                return JBPF_LCM_CLI_INVALID_ARGS;
            }
            (*opts).typ = load;
            set_direction = true;
            continue;
        }

        case 'u': {
            if (set_direction) {
                cout << "Invalid arguments: cannot specify both load and unload" << endl;
                return JBPF_LCM_CLI_INVALID_ARGS;
            }
            (*opts).typ = unload;
            set_direction = true;
            continue;
        }

        case '?':
        case 'h':
        default:
            cout << "Usage jbpf_lcm_cli [options]" << endl
                 << "Options:" << endl
                 << "-a <address>\tjbpf lcm ipc address (defaults to " << default_address.str() << ")" << endl
                 << "-c <config>\tcodeletset configuration file" << endl
                 << "-l\t\tload codeletset" << endl
                 << "-u\t\tunload codeletset" << endl
                 << endl;

            printf("Help/Usage Example\n");
            return JBPF_LCM_CLI_EXIT;

        case -1:
            break;
        }

        break;
    }

    address.copy(opts->addr.path, JBPF_LCM_IPC_NAME_LEN - 1);
    opts->addr.path[address.length()] = '\0';

    if (!exists(filename)) {
        cout << "File " << filename << " does not exist" << endl;
        return JBPF_LCM_CLI_INVALID_ARGS;
    }

    if (!set_direction) {
        cout << "Invalid arguments: must specify either -l or -u" << endl;
        return JBPF_LCM_CLI_INVALID_ARGS;
    }

    YAML::Node cfg = YAML::LoadFile(filename);

    jbpf_lcm_cli::parser::parse_req_outcome parse_ret;
    switch ((*opts).typ) {
    case load: {
        vector<string> codeletset_elems;
        codeletset_elems.push_back(address);
        parse_ret = jbpf_lcm_cli::parser::parse_jbpf_codeletset_load_req(cfg, &(*opts).req.load, codeletset_elems);
        break;
    }

    case unload:
        parse_ret = jbpf_lcm_cli::parser::parse_jbpf_codeletset_unload_req(cfg, &(*opts).req.unload);
        break;
    }

    switch (parse_ret) {
    case jbpf_lcm_cli::parser::JBPF_LCM_PARSE_REQ_FAILED:
    default:
        return JBPF_LCM_CLI_PARSE_FAILED;
    case jbpf_lcm_cli::parser::JBPF_LCM_PARSE_REQ_SUCCESS:
        return JBPF_LCM_CLI_REQ_SUCCESS;
    case jbpf_lcm_cli::parser::JBPF_LCM_PARSE_VERIFIER_FAILED:
        return JBPF_LCM_CLI_VERIFIER_FAILED;
    }
}

load_req_outcome
run_loader(int ac, char** av)
{
    lcm_cli_config conf = {0};
    auto parse_ret = parseArgs(ac, av, &conf);
    if (parse_ret == JBPF_LCM_CLI_EXIT)
        return JBPF_LCM_CLI_REQ_SUCCESS;
    if (parse_ret != JBPF_LCM_CLI_REQ_SUCCESS)
        return parse_ret;

    auto ret = 0;
    switch (conf.typ) {
    case load:
        cout << "Loading codelet set: " << conf.req.load.codeletset_id.name << endl;
        ret = jbpf_lcm_ipc_send_codeletset_load_req(&conf.addr, &conf.req.load);
        break;
    case unload:
        cout << "Unloading codelet set: " << conf.req.unload.codeletset_id.name << endl;
        ret = jbpf_lcm_ipc_send_codeletset_unload_req(&conf.addr, &conf.req.unload);
        break;
    }

    switch (ret) {
    case JBPF_LCM_IPC_REQ_SUCCESS:
        return JBPF_LCM_CLI_REQ_SUCCESS;
    case JBPF_LCM_IPC_REQ_FAIL:
    default:
        cout << "Request failed with response " << ret << endl;
        return JBPF_LCM_CLI_REQ_FAILURE;
    }
}

load_req_outcome
run_lcm_subproc(const std::vector<std::string>& str_args, const std::string& inline_bin_arg)
{
    // loader reads opts from argv using getopts
    // so we need to reset the getopts global to start from the beginning
    optind = 1;

    size_t argc = str_args.size() + 1;
    // Use unique_ptr with custom deleter for the char* array
    std::unique_ptr<char*[]> args(new char*[argc]);

    // Allocate and copy the inline binary argument
    args[0] = new char[inline_bin_arg.length() + 1];
    strcpy(args[0], inline_bin_arg.c_str());

    // Smart pointers for individual strings to ensure proper cleanup
    std::vector<std::unique_ptr<char[]>> arg_strings;
    arg_strings.reserve(str_args.size());

    for (size_t i = 0; i < str_args.size(); ++i) {
        auto cstr = std::make_unique<char[]>(str_args[i].length() + 1);
        strcpy(cstr.get(), str_args[i].c_str());
        args[i + 1] = cstr.get();
        arg_strings.push_back(std::move(cstr));
    }

    auto ret = jbpf_lcm_cli::loader::run_loader(static_cast<int>(argc), args.get());

    // Memory cleanup is automatic via smart pointers
    return ret;
}
} // namespace loader
} // namespace jbpf_lcm_cli
