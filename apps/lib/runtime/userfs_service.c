#include "userfs.h"
#include "syscall.h"
#include "libc.h"
#include <stdint.h>
#include <stddef.h>

static userfs_config_t g_userfs_config;
static int g_userfs_channel = -1;
static int g_userfs_running = 0;

int userfs_service_start(const userfs_config_t *config)
{
    int ret;
    
    if (config == NULL || config->fs_type == NULL || config->ops == NULL) {
        printf("[userfs] invalid config\n");
        return -1;
    }
    
    memcpy(&g_userfs_config, config, sizeof(userfs_config_t));
    
    g_userfs_channel = pup_channel_create();
    if (g_userfs_channel < 0) {
        printf("[userfs] failed to create channel\n");
        return -1;
    }
    
    ret = userfs_register(config->fs_type, g_userfs_channel);
    if (ret < 0) {
        printf("[userfs] failed to register fs type '%s'\n", config->fs_type);
        pup_channel_destroy(g_userfs_channel);
        g_userfs_channel = -1;
        return -1;
    }
    
    printf("[userfs] service '%s' started (ch=%d)\n", config->fs_type, g_userfs_channel);
    
    g_userfs_running = 1;
    
    while (g_userfs_running) {
        pup_channel_msg_t msg;
        int cookie;
        pup_userfs_req_header_t *req;
        long result = 0;
        
        cookie = pup_channel_receive(g_userfs_channel, &msg);
        if (cookie < 0) {
            sched_yield();
            continue;
        }
        
        if (msg.buf == NULL || msg.buf_sz < sizeof(pup_userfs_req_header_t)) {
            pup_channel_reply(g_userfs_channel, cookie, -1);
            continue;
        }
        
        req = (pup_userfs_req_header_t *)msg.buf;
        
        switch (req->op) {
        case PUP_USERFS_OP_MOUNT: {
            pup_userfs_mount_req_t *mount_req = (pup_userfs_mount_req_t *)req;
            if (g_userfs_config.ops->mount) {
                result = g_userfs_config.ops->mount(
                    mount_req->device[0] ? mount_req->device : NULL,
                    mount_req->mount_flags,
                    NULL);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_UNMOUNT:
            if (g_userfs_config.ops->unmount) {
                result = g_userfs_config.ops->unmount();
            } else {
                result = 0;
            }
            break;
        
        case PUP_USERFS_OP_OPEN: {
            pup_userfs_open_req_t *open_req = (pup_userfs_open_req_t *)req;
            if (g_userfs_config.ops->open) {
                result = g_userfs_config.ops->open(open_req->path, open_req->flags);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_CLOSE:
            if (g_userfs_config.ops->close) {
                result = g_userfs_config.ops->close(req->fd);
            } else {
                result = 0;
            }
            break;
        
        case PUP_USERFS_OP_READ: {
            pup_userfs_io_req_t *io_req = (pup_userfs_io_req_t *)req;
            if (g_userfs_config.ops->read) {
                result = g_userfs_config.ops->read(req->fd, io_req->data_buf, io_req->size);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_WRITE: {
            pup_userfs_io_req_t *io_req = (pup_userfs_io_req_t *)req;
            if (g_userfs_config.ops->write) {
                result = g_userfs_config.ops->write(req->fd, io_req->data_buf, io_req->size);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_LSEEK: {
            pup_userfs_lseek_req_t *lseek_req = (pup_userfs_lseek_req_t *)req;
            if (g_userfs_config.ops->lseek) {
                result = g_userfs_config.ops->lseek(req->fd, lseek_req->offset, lseek_req->whence);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_SYNC:
            if (g_userfs_config.ops->sync) {
                result = g_userfs_config.ops->sync(req->fd);
            } else {
                result = 0;
            }
            break;
        
        case PUP_USERFS_OP_STAT: {
            pup_userfs_stat_req_t *stat_req = (pup_userfs_stat_req_t *)req;
            pup_userfs_stat_resp_t *stat_resp = (pup_userfs_stat_resp_t *)msg.buf;
            if (g_userfs_config.ops->stat) {
                result = g_userfs_config.ops->stat(stat_req->path, stat_resp);
            } else {
                result = -1;
            }
            stat_resp->header.result = (result >= 0) ? 0 : -1;
            pup_channel_reply(g_userfs_channel, cookie, result);
            continue;
        }
        
        case PUP_USERFS_OP_UNLINK: {
            pup_userfs_unlink_req_t *unlink_req = (pup_userfs_unlink_req_t *)req;
            if (g_userfs_config.ops->unlink) {
                result = g_userfs_config.ops->unlink(unlink_req->path);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_RENAME: {
            pup_userfs_rename_req_t *rename_req = (pup_userfs_rename_req_t *)req;
            if (g_userfs_config.ops->rename) {
                result = g_userfs_config.ops->rename(rename_req->oldpath, rename_req->newpath);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_MKDIR: {
            pup_userfs_mkdir_req_t *mkdir_req = (pup_userfs_mkdir_req_t *)req;
            if (g_userfs_config.ops->mkdir) {
                result = g_userfs_config.ops->mkdir(mkdir_req->path);
            } else {
                result = -1;
            }
            break;
        }
        
        case PUP_USERFS_OP_GETDENTS: {
            pup_userfs_getdents_req_t *getdents_req = (pup_userfs_getdents_req_t *)req;
            pup_userfs_dirent_t *dirp = (pup_userfs_dirent_t *)(msg.buf + sizeof(pup_userfs_resp_header_t));
            int num_entries = 0;
            
            if (g_userfs_config.ops->getdents) {
                result = g_userfs_config.ops->getdents(req->fd, dirp, getdents_req->count, &num_entries);
                if (result >= 0) {
                    pup_userfs_resp_header_t *hdr = (pup_userfs_resp_header_t *)msg.buf;
                    hdr->result = num_entries;
                    pup_channel_reply(g_userfs_channel, cookie, num_entries);
                    continue;
                }
            }
            result = -1;
            break;
        }
        
        case PUP_USERFS_OP_STATFS: {
            pup_userfs_statfs_resp_t *statfs_resp = (pup_userfs_statfs_resp_t *)msg.buf;
            if (g_userfs_config.ops->statfs) {
                result = g_userfs_config.ops->statfs(statfs_resp);
            } else {
                result = -1;
            }
            statfs_resp->header.result = (result >= 0) ? 0 : -1;
            pup_channel_reply(g_userfs_channel, cookie, result);
            continue;
        }
        
        case PUP_USERFS_OP_MKFS: {
            pup_userfs_mkfs_req_t *mkfs_req = (pup_userfs_mkfs_req_t *)req;
            if (g_userfs_config.ops->mkfs) {
                result = g_userfs_config.ops->mkfs(
                    mkfs_req->device[0] ? mkfs_req->device : NULL,
                    mkfs_req->block_size,
                    mkfs_req->block_count);
            } else {
                result = -1;
            }
            break;
        }
        
        default:
            result = -1;
            break;
        }
        
        pup_channel_reply(g_userfs_channel, cookie, result);
    }
    
    userfs_unregister(config->fs_type);
    pup_channel_destroy(g_userfs_channel);
    g_userfs_channel = -1;
    
    return 0;
}

void userfs_service_stop(void)
{
    g_userfs_running = 0;
}
