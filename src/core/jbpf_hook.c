// Copyright (c) Microsoft Corporation. All rights reserved.

#include <pthread.h>

#include "jbpf_hook_defs.h"
#include "jbpf_int.h"
#include "jbpf_hook.h"
#include "jbpf_memory.h"
#include "jbpf_logging.h"
#include "jbpf_common_types.h"

static pthread_mutex_t hook_mutex = PTHREAD_MUTEX_INITIALIZER;

CK_EPOCH_CONTAINER(struct jbpf_hook_codelet, epoch_entry, epoch_container)

static void
free_codelet_list(ck_epoch_entry_t* e)
{
    struct jbpf_hook_codelet* node = epoch_container(e);
    jbpf_free_mem(node);
}

int
jbpf_register_codelet_hook(
    struct jbpf_hook* hook,
    jbpf_jit_fn codelet,
    jbpf_runtime_threshold_t runtime_threshold,
    jbpf_codelet_priority_t prio)
{
    struct jbpf_hook_codelet hook_codelet;
    struct jbpf_hook_codelet *old_codelets, *new_codelets;

    int ret = -1;
    int nr_codelets = 0;
    int pos = -1;

    if (!codelet) {
        return -1;
    }

    /* Only one thread can update programs */
    pthread_mutex_lock(&hook_mutex);

    hook_codelet.jbpf_codelet = codelet;
    hook_codelet.prio = prio;
    hook_codelet.time_thresh = runtime_threshold;

    ck_epoch_begin(e_record, NULL);
    old_codelets = hook->codelets;

    /* Check where we should place the new program based on its priority and
     * make sure that program has not been registered already */
    if (old_codelets) {

        for (nr_codelets = 0; old_codelets[nr_codelets].jbpf_codelet; nr_codelets++) {
            if (pos < 0 && old_codelets[nr_codelets].prio < prio) {
                pos = nr_codelets;
            }
            /* The program already exists. Abort */
            if (old_codelets[nr_codelets].jbpf_codelet == codelet) {
                jbpf_logger(JBPF_INFO, "Codelet already registered to %s\n", hook->name);
                ck_pr_store_ptr(&hook->codelets, old_codelets);
                ck_epoch_end(e_record, NULL);
                goto out;
            }
        }

        /* If this is a control hook, only one program is allowed to be loaded */
        if (hook->hook_type == JBPF_HOOK_TYPE_CTRL && nr_codelets > 0) {
            jbpf_logger(JBPF_ERROR, "This is a control hook. Only one codelet can be loaded\n");
            ck_epoch_end(e_record, NULL);
            goto out;
        }
    }

    // +2 to add the new codelet and the NULL terminator
    new_codelets = (struct jbpf_hook_codelet*)jbpf_calloc_mem(nr_codelets + 2, sizeof(struct jbpf_hook_codelet));

    if (!new_codelets) {
        ck_epoch_end(e_record, NULL);
        jbpf_logger(JBPF_ERROR, "Failed to allocate memory for new codelets\n");
        goto out;
    }

    /* If programs were already registered */
    if (old_codelets) {
        if (pos < 0) {
            pos = nr_codelets;
            memcpy(new_codelets, old_codelets, nr_codelets * sizeof(struct jbpf_hook_codelet));
        } else {
            /* Copy the data before and after the new position */
            memcpy(new_codelets, old_codelets, pos * sizeof(struct jbpf_hook_codelet));
            memcpy(new_codelets + pos + 1, old_codelets + pos, (nr_codelets - pos) * sizeof(struct jbpf_hook_codelet));
        }
    } else {
        pos = 0;
    }

    // cppcheck-suppress uninitStructMember
    new_codelets[pos] = hook_codelet;
    new_codelets[nr_codelets + 1].jbpf_codelet = NULL;

    ck_pr_store_ptr(&hook->codelets, new_codelets);

    // debug prints
    jbpf_logger(JBPF_INFO, "Codelet %p registered to %s: %ld\n", codelet, hook->name, (u_int64_t)&hook->codelets);
    jbpf_logger(JBPF_INFO, "The number of codelets registered to %s: %d\n", hook->name, nr_codelets + 1);


    ck_epoch_end(e_record, NULL);
    if (old_codelets) {
        ck_epoch_call_strict(e_record, &old_codelets->epoch_entry, free_codelet_list);
        ck_epoch_barrier(e_record);
    }

    ret = 0;
out:
    pthread_mutex_unlock(&hook_mutex);
    return ret;
}

int
jbpf_remove_codelet_hook(struct jbpf_hook* hook, jbpf_jit_fn codelet)
{

    struct jbpf_hook_codelet *old_codelets, *new_codelets;
    int nr_codelets = 0;
    int nr_rem = 0;
    int i, pos;
    int ret = -1;

    /* Only one thread can update programs */
    pthread_mutex_lock(&hook_mutex);

    ck_epoch_begin(e_record, NULL);
    old_codelets = hook->codelets;

    /* Find the position of the program to remove */
    if (old_codelets) {
        for (nr_codelets = 0; old_codelets[nr_codelets].jbpf_codelet; nr_codelets++) {
            if (old_codelets[nr_codelets].jbpf_codelet == codelet) {
                nr_rem++;
            }
        }
    } else { /* No program is registered */
        ck_epoch_end(e_record, NULL);
        goto out;
    }

    /* If we did not find the program */
    if (nr_rem == 0) {
        ck_epoch_end(e_record, NULL);
        goto out;
    }

    /* All programs are removed */
    if (nr_codelets - nr_rem == 0) {
        /* Need to disable the hook */
        ck_pr_store_ptr(&hook->codelets, NULL);
    } else {
        new_codelets =
            (struct jbpf_hook_codelet*)jbpf_calloc_mem(nr_codelets - nr_rem + 1, sizeof(struct jbpf_hook_codelet));

        if (!new_codelets) {
            ck_epoch_end(e_record, NULL);
            goto out;
        }

        /* Need to copy old programs to new structure */
        pos = 0;
        for (i = 0; old_codelets[i].jbpf_codelet; i++) {
            if (old_codelets[i].jbpf_codelet != codelet) {
                new_codelets[pos++] = old_codelets[i];
            }
        }
        new_codelets[nr_codelets - nr_rem].jbpf_codelet = NULL;
        ck_pr_store_ptr(&hook->codelets, new_codelets);
    }
    ck_epoch_end(e_record, NULL);
    ck_epoch_synchronize(e_record);
    jbpf_free_mem(old_codelets);

    ret = 0;

out: 
    pthread_mutex_unlock(&hook_mutex);
    return ret;
}
