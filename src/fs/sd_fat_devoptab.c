/***************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/

//THIS IS MODIFIED FROM THE ORIGONAL SOURCE! headers have been changed and functions removed

#include <errno.h>
#include <sys/statvfs.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#include <coreinit/all_headers.h>

#include "fs_utils.h"
#include "../common/fs_defs.h"

static int sd_fat_add_device (const char *name, const char *mount_path, void *pClient, void *pCmd)
{
    devoptab_t *dev = NULL;
    char *devname = NULL;
    char *devpath = NULL;
    int i;

    // Sanity check
    if (!name) {
        errno = EINVAL;
        return -1;
    }

    // Allocate a devoptab for this device
    dev = (devoptab_t *) malloc(sizeof(devoptab_t) + strlen(name) + 1);
    if (!dev) {
        errno = ENOMEM;
        return -1;
    }

    // Use the space allocated at the end of the devoptab for storing the device name
    devname = (char*)(dev + 1);
    strcpy(devname, name);

    // create private data
    sd_fat_private_t *priv = (sd_fat_private_t *)malloc(sizeof(sd_fat_private_t) + strlen(mount_path) + 1);
    if(!priv) {
        free(dev);
        errno = ENOMEM;
        return -1;
    }

    devpath = (char*)(priv+1);
    strcpy(devpath, mount_path);

    // setup private data
    priv->mount_path = devpath;
    priv->pClient = pClient;
    priv->pCmd = pCmd;
    priv->pMutex = malloc(OS_MUTEX_SIZE);

    if(!priv->pMutex) {
        free(dev);
        free(priv);
        errno = ENOMEM;
        return -1;
    }

    OSInitMutex(priv->pMutex);

    // Setup the devoptab
    memcpy(dev, &devops_sd_fat, sizeof(devoptab_t));
    dev->name = devname;
    dev->deviceData = priv;

    // Add the device to the devoptab table (if there is a free slot)
    for (i = 3; i < STD_MAX; i++) {
        if (devoptab_list[i] == devoptab_list[0]) {
            devoptab_list[i] = dev;
            return 0;
        }
    }

    // failure, free all memory
    free(priv);
    free(dev);

    // If we reach here then there are no free slots in the devoptab table for this device
    errno = EADDRNOTAVAIL;
    return -1;
}

static int sd_fat_remove_device (const char *path, void **pClient, void **pCmd, char **mountPath)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    // Get the device name from the path
    strncpy(name, path, 127);
    strtok(name, ":/");

    // Find and remove the specified device from the devoptab table
    // NOTE: We do this manually due to a 'bug' in RemoveDevice
    //       which ignores names with suffixes and causes names
    //       like "ntfs" and "ntfs1" to be seen as equals
    for (i = 3; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                devoptab_list[i] = devoptab_list[0];

                if(devoptab->deviceData)
                {
                    sd_fat_private_t *priv = (sd_fat_private_t *)devoptab->deviceData;
                    *pClient = priv->pClient;
                    *pCmd = priv->pCmd;
                    *mountPath = (char*) malloc(strlen(priv->mount_path)+1);
                    if(*mountPath)
                        strcpy(*mountPath, priv->mount_path);
                    if(priv->pMutex)
                        free(priv->pMutex);
                    free(devoptab->deviceData);
                }

                free((devoptab_t*)devoptab);
                return 0;
            }
        }
    }

    return -1;
}


int mount_sd_fat(const char *path)
{
    int result = -1;

    // get command and client
    void* pClient = malloc(FS_CLIENT_SIZE);
    void* pCmd = malloc(FS_CMD_BLOCK_SIZE);

    if(!pClient || !pCmd) {
        // just in case free if not 0
        if(pClient)
            free(pClient);
        if(pCmd)
            free(pCmd);
        return -2;
    }

    FSInit();
    FSInitCmdBlock(pCmd);
    FSAddClientEx(pClient, 0, -1);

    char *mountPath = NULL;

    if(MountFS(pClient, pCmd, &mountPath) == 0) {
        result = sd_fat_add_device(path, mountPath, pClient, pCmd);
        free(mountPath);
    }

    return result;
}

int unmount_sd_fat(const char *path)
{
    void *pClient = 0;
    void *pCmd = 0;
    char *mountPath = 0;

    int result = sd_fat_remove_device(path, &pClient, &pCmd, &mountPath);
    if(result == 0)
    {
        UmountFS(pClient, pCmd, mountPath);
        FSErrorFlag flag;
        FSDelClient(pClient, flag);
        free(pClient);
        free(pCmd);
        free(mountPath);
        //FSShutdown();
    }
    return result;
}
