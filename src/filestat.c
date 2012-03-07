#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dcp.h"
#include "filestat.h"
#include "log.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel DCOPY_debug_level;

void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    static struct stat st;
    static int status;
    int is_top_dir = !strcmp(op->operand, DCOPY_user_opts.src_path[0]);
    char path[4096];

    if(is_top_dir) {
        sprintf(path, "%s", DCOPY_user_opts.src_path[0]);
    }
    else {
        sprintf(path, "%s/%s", DCOPY_user_opts.src_path[0], op->operand);
    }

    status = lstat(path, &st);

    if(status != EXIT_SUCCESS) {
        LOG(DCOPY_LOG_ERR, "Unable to stat \"%s\"", path);
        perror("stat");
    }
    else if(S_ISDIR(st.st_mode) && !(S_ISLNK(st.st_mode))) {
        char dir[2048];
        LOG(DCOPY_LOG_DBG, "Operand: %s Dir: %s", op->operand, DCOPY_user_opts.dest_path);

        if(is_top_dir) {
            sprintf(dir, "mkdir -p %s", op->operand);
        }
        else {
            sprintf(dir, "mkdir -p %s/%s", DCOPY_user_opts.dest_path, op->operand);
        }

        LOG(DCOPY_LOG_DBG, "Creating %s", dir);

        FILE* p = popen(dir, "r");
        pclose(p);
        DCOPY_process_dir(op->operand, handle);
    }
    else {
        int num_chunks = st.st_size / DCOPY_CHUNK_SIZE;
        LOG(DCOPY_LOG_DBG, "File size: %ld Chunks:%d Total: %d", st.st_size, num_chunks, num_chunks * DCOPY_CHUNK_SIZE);
        int i = 0;

        for(i = 0; i < num_chunks; i++) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand);
            handle->enqueue(newop);
            free(newop);
        }

        if(num_chunks * DCOPY_CHUNK_SIZE < st.st_size) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand);
            handle->enqueue(newop);
            free(newop);
        }
    }

    return;
}

void DCOPY_process_dir(char* dir, CIRCLE_handle* handle)
{
    DIR* current_dir;
    char parent[2048];
    struct dirent* current_ent;
    char path[4096];
    int is_top_dir = !strcmp(dir, DCOPY_user_opts.src_path[0]);

    if(is_top_dir) {
        sprintf(path, "%s", dir);
    }
    else {
        sprintf(path, "%s/%s", DCOPY_user_opts.src_path[0], dir);
    }

    current_dir = opendir(path);

    if(!current_dir) {
        LOG(DCOPY_LOG_ERR, "Unable to open dir: %s", path);
    }
    else {
        /* Read in each directory entry */
        while((current_ent = readdir(current_dir)) != NULL) {
            /* We don't care about . or .. */
            if((strncmp(current_ent->d_name, ".", 2)) && (strncmp(current_ent->d_name, "..", 3))) {
                LOG(DCOPY_LOG_DBG, "Dir entry %s / %s", dir, current_ent->d_name);

                if(is_top_dir) {
                    strcpy(parent, "");
                }
                else {
                    strcpy(parent, dir);
                }

                LOG(DCOPY_LOG_DBG, "Parent %s", parent);

                strcat(parent, "/");
                strcat(parent, current_ent->d_name);

                LOG(DCOPY_LOG_DBG, "Pushing [%s] <- [%s]", parent, dir);

                char* newop = DCOPY_encode_operation(STAT, 0, parent);
                handle->enqueue(newop);
                free(newop);
            }
        }
    }

    closedir(current_dir);
    return;
}

/* EOF */