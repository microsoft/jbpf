// Copyright (c) Microsoft Corporation. All rights reserved.
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "common.h"

boost::property_tree::ptree
toPtree(const Packet& packet)
{
    boost::property_tree::ptree pt;
    pt.put("seq_no", packet.seq_no);
    pt.put("value", packet.value);
    pt.put("name", std::string(packet.name));
    return pt;
}

std::string
toJson(const boost::property_tree::ptree& pt)
{
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    return oss.str();
};

size_t
copy_string_to_c_array(const std::string& source, char* destination, size_t max_len)
{
    size_t length_to_copy = std::min(source.size(), max_len - 1);
    strncpy(destination, source.c_str(), length_to_copy);
    destination[length_to_copy] = '\0';
    return length_to_copy;
}

#ifdef __cplusplus
extern "C"
{
#endif

    int
    jbpf_io_serialize(
        void* input_msg_buf, size_t input_msg_buf_size, char* serialized_data_buf, size_t serialized_data_buf_size)
    {
        Packet p = *static_cast<Packet*>(input_msg_buf);

        boost::property_tree::ptree pt = toPtree(p);
        std::string json = toJson(pt);

        size_t len = copy_string_to_c_array(json, serialized_data_buf, serialized_data_buf_size);
        return len;
    }

#ifdef __cplusplus
}
#endif
