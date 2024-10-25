#pragma once

struct Packet
{
    int seq_no;
    int value;
    char name[32];
};

struct PacketResp
{
    int seq_no;
};