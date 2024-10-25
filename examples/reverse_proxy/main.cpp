
#include "listener.hpp"
#include "parser.hpp"
#include "jbpf_verifier.hpp"

int
main(int ac, char** av)
{
    // register the "jbpf_verify" function
    jbpf_reverse_proxy::parser::parser_jbpf_set_verify_func(jbpf_verify);

    // run the listener
    return jbpf_reverse_proxy::listener::run_listener(ac, av);
}