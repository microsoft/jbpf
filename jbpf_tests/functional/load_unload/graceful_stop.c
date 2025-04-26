/*
 * The purpose of this test is to ensure that jbpf_stop() will exit gracefully even if there are numerous resources
 * created and active.
 *
 * This test does the following:
 * 1. It loads 8 codeletSets, where each codeletSet has 8 codelets, giving a total of the maximum 64 codelets
 * 2  Each codelet has the maximum number of output channels (5).  This givas a total of 320 output channels.
 * 3. Once all the codeletSets are loaded, the hook is called 100 times.
 * 4. The code immediately calls jbpf_stop().
 * 5. There is an assert at the end of the test to show we do not crash.
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"
#include "jbpf_test_lib.h"

#define NUM_ITERATIONS 1000
#define TOTAL_HOOK_CALLS (NUM_ITERATIONS * JBPF_MAX_CODELETS_IN_CODELETSET)

#define NUM_CODELET_SETS (8)
#define NUM_CODELETS_IN_CODELETSET (JBPF_MAX_CODELETS_IN_CODELETSET)
#define NUM_OUT_CHANNELS_PER_CODELET (JBPF_MAX_IO_CHANNEL)

jbpf_io_stream_id_t stream_ids[NUM_CODELET_SETS * NUM_CODELETS_IN_CODELETSET * NUM_OUT_CHANNELS_PER_CODELET] = {0};

void
jbpf_codelet_descriptor_to_str(jbpf_codeletset_load_req_s* req, char* out, uint16_t outlen)
{
    int offset = snprintf(out, outlen, "Codeletset: %s\n", req->codeletset_id.name);
    for (int i = 0; i < req->num_codelet_descriptors; i++) {
        jbpf_codelet_descriptor_s* desc = &req->codelet_descriptor[i];
        offset += snprintf(out + offset, outlen - offset, "Codelet [%d]: %s\n", i, desc->codelet_name);
        offset += snprintf(out + offset, outlen - offset, "Hook: %s\n", desc->hook_name);
        offset += snprintf(out + offset, outlen - offset, "Path: %s\n", desc->codelet_path);
        offset += snprintf(out + offset, outlen - offset, "Priority: %d\n", desc->priority);
        offset += snprintf(out + offset, outlen - offset, "Runtime Threshold: %lu\n", desc->runtime_threshold);
        offset += snprintf(out + offset, outlen - offset, "Num In IO Channels: %d\n", desc->num_in_io_channel);
        for (int j = 0; j < desc->num_in_io_channel; j++) {
            jbpf_io_channel_desc_s* ch = &desc->in_io_channel[j];
            offset += snprintf(out + offset, outlen - offset, "In IO Channel [%d]: %s\n", j, ch->name);
            offset += snprintf(out + offset, outlen - offset, "In IO Channel [%d] Stream ID: ", j);
            for (int k = 0; k < JBPF_STREAM_ID_LEN; k++) {
                offset += snprintf(out + offset, outlen - offset, "%02x", ch->stream_id.id[k]);
            }
            offset += snprintf(out + offset, outlen - offset, "\n");
            offset += snprintf(
                out + offset,
                outlen - offset,
                "In IO [%d] Channel Serde: %s\n",
                j,
                ch->has_serde ? ch->serde.file_path : "None");
        }
        offset += snprintf(out + offset, outlen - offset, "Num Out IO Channels: %d\n", desc->num_out_io_channel);
        for (int j = 0; j < desc->num_out_io_channel; j++) {
            jbpf_io_channel_desc_s* ch = &desc->out_io_channel[j];
            offset += snprintf(out + offset, outlen - offset, "Out IO Channel [%d]: %s\n", j, ch->name);
            offset += snprintf(out + offset, outlen - offset, "Out IO Channel [%d] Stream ID: ", j);
            for (int k = 0; k < JBPF_STREAM_ID_LEN; k++) {
                offset += snprintf(out + offset, outlen - offset, "%02x", ch->stream_id.id[k]);
            }
            offset += snprintf(out + offset, outlen - offset, "\n");
            offset += snprintf(
                out + offset,
                outlen - offset,
                "Out IO [%d] Channel Serde: %s\n",
                j,
                ch->has_serde ? ch->serde.file_path : "None");
        }
        offset += snprintf(out + offset, outlen - offset, "Num Linked maps: %d\n", desc->num_linked_maps);
        for (int j = 0; j < desc->num_linked_maps; j++) {
            jbpf_linked_map_descriptor_s* lmap = &desc->linked_maps[j];
            offset += snprintf(out + offset, outlen - offset, "Lmap[%d]: map_name %s \n", j, lmap->map_name);
            offset += snprintf(
                out + offset, outlen - offset, "Lmap[%d]: linked_codelet_name %s \n", j, lmap->linked_codelet_name);
            offset +=
                snprintf(out + offset, outlen - offset, "Lmap[%d]: linked_map_name %s \n", j, lmap->linked_map_name);
        }
    }
}

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    return;
    // int* output;

    for (int i = 0; i < num_bufs; i++) {

        // printf("stream = ");
        // for (int j = 0; j < JBPF_STREAM_ID_LEN; j++) {
        //     printf("%02x", stream_id->id[j]);
        // }
        // printf("\n");

        // get stream idx
        int s;
        for (s = 0; s < sizeof(stream_ids) / sizeof(stream_ids[0]); s++) {
            if (memcmp(stream_id, &stream_ids[s], sizeof(jbpf_io_stream_id_t)) == 0) {
                break;
            }
        }
        if (s == sizeof(stream_ids) / sizeof(stream_ids[0])) {
            // stream not found
            assert(false);
        }
        // output = bufs[i];
        // printf("s=%d buf=%d\n", s, *output);
    }
}

int
main(int argc, char** argv)
{
    jbpf_codeletset_load_req_s codeletset_req[NUM_CODELET_SETS] = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    __assert__(jbpf_init(&config) == 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    for (uint16_t codset = 0; codset < NUM_CODELET_SETS; codset++) {

        jbpf_codeletset_load_req_s* codset_req = &codeletset_req[codset];

        // The name of the codeletset
        snprintf(codset_req->codeletset_id.name, JBPF_CODELETSET_NAME_LEN, "max_output_codeletset%d", codset);

        codset_req->num_codelet_descriptors = NUM_CODELETS_IN_CODELETSET;

        for (uint16_t cod = 0; cod < codset_req->num_codelet_descriptors; cod++) {

            jbpf_codelet_descriptor_s* cod_desc = &codset_req->codelet_descriptor[cod];

            cod_desc->num_out_io_channel = NUM_OUT_CHANNELS_PER_CODELET;

            for (int outCh = 0; outCh < cod_desc->num_out_io_channel; outCh++) {

                // The name of the output map that corresponds to the output channel of the codelet
                jbpf_io_channel_name_t ch_name = {0};
                snprintf(ch_name, JBPF_IO_CHANNEL_NAME_LEN, "output_map%d", outCh);
                strcpy(cod_desc->out_io_channel[outCh].name, ch_name);

                // Link the map to a stream id
                uint16_t stream_id = (codset * NUM_CODELETS_IN_CODELETSET * NUM_OUT_CHANNELS_PER_CODELET) +
                                     (cod * NUM_OUT_CHANNELS_PER_CODELET) + outCh;
                // printf("stream_id = %d codeset = %d cod = %d outCh = %d\n", stream_id, codset, cod, outCh);
                uint8_t* stream_id_ptr = (uint8_t*)&stream_id;
                for (int b = 0; b < JBPF_STREAM_ID_LEN / 2; b++) {
                    stream_ids[stream_id].id[b * 2] = stream_id_ptr[0];
                    stream_ids[stream_id].id[b * 2 + 1] = stream_id_ptr[1];
                }

                memcpy(&cod_desc->out_io_channel[outCh].stream_id, &stream_ids[stream_id], JBPF_STREAM_ID_LEN);

                // The output channel of the codelet does not have a serializer
                cod_desc->out_io_channel[outCh].has_serde = false;
            }

            cod_desc->num_in_io_channel = 0;
            cod_desc->num_linked_maps = 0;

            // The path of the codelet
            __assert__(jbpf_path != NULL);
            snprintf(
                cod_desc->codelet_path,
                JBPF_PATH_LEN,
                "%s/jbpf_tests/test_files/codelets/max_output_shared/max_output_shared.o",
                jbpf_path);
            snprintf(cod_desc->codelet_name, JBPF_CODELET_NAME_LEN, "max_output_shared%d", cod);
            strcpy(cod_desc->hook_name, "test1");
        }
    }

    // Load the codeletsets
    for (uint16_t codset = 0; codset < NUM_CODELET_SETS; codset++) {
        jbpf_codeletset_load_req_s* codset_req = &codeletset_req[codset];

        JBPF_UNUSED(codset_req);
        // static char ddd[10000];
        // jbpf_codelet_descriptor_to_str(codset_req, ddd, sizeof(ddd));
        // printf("\n----------------------------\n%s\n", ddd);

        // printf("Loading codeletset %s\n", codset_req->codeletset_id.name);
        __assert__(jbpf_codeletset_load(codset_req, NULL) == JBPF_CODELET_LOAD_SUCCESS);
    }

    usleep(1000000);

    // Call the hooks 5 times and check that we got the expected data
    struct packet p = {0, 0};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        hook_test1(&p, i);
    }

    // Cleanup and stop
    jbpf_stop();

    assert(true);

    printf("Test completed successfully\n");
    return 0;
}
