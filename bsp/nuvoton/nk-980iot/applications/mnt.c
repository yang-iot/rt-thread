/**************************************************************************//**
*
* @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author       Notes
* 2020-12-12      Wayne        First version
*
******************************************************************************/

#include "rtconfig.h"

#include <rtthread.h>

#define LOG_TAG         "mnt"
#define DBG_ENABLE
#define DBG_SECTION_NAME "mnt"
#define DBG_LEVEL DBG_ERROR
#define DBG_COLOR
#include <rtdbg.h>

#if defined(RT_USING_DFS)
    #include <dfs_fs.h>
    #include <dfs_posix.h>
#endif

#if defined(PKG_USING_FAL)
    #include <fal.h>
#endif

#if defined(PKG_USING_RAMDISK)
    #define RAMDISK_NAME         "ramdisk0"
    #define MOUNT_POINT_RAMDISK0 "/"
#endif

#if defined(BOARD_USING_STORAGE_SPIFLASH)
    #define PARTITION_NAME_FILESYSTEM "filesystem"
    #define MOUNT_POINT_SPIFLASH0 "/mnt/"PARTITION_NAME_FILESYSTEM
#endif

#if defined(PKG_USING_RAMDISK)

/* Recursive mkdir */
static int mkdir_p(const char *dir, const mode_t mode)
{
    int ret = -1;
    char *tmp = NULL;
    char *p = NULL;
    struct stat sb;
    rt_size_t len;

    if (!dir)
        goto exit_mkdir_p;

    /* Copy path */
    /* Get the string length */
    len = strlen(dir);
    tmp = rt_strdup(dir);

    /* Remove trailing slash */
    if (tmp[len - 1] == '/')
    {
        tmp[len - 1] = '\0';
        len--;
    }

    /* check if path exists and is a directory */
    if (stat(tmp, &sb) == 0)
    {
        if (S_ISDIR(sb.st_mode))
        {
            ret = 0;
            goto exit_mkdir_p;
        }
    }

    /* Recursive mkdir */
    for (p = tmp + 1; p - tmp <= len; p++)
    {
        if ((*p == '/') || (p - tmp == len))
        {
            *p = 0;

            /* Test path */
            if (stat(tmp, &sb) != 0)
            {
                /* Path does not exist - create directory */
                if (mkdir(tmp, mode) < 0)
                {
                    goto exit_mkdir_p;
                }
            }
            else if (!S_ISDIR(sb.st_mode))
            {
                /* Not a directory */
                goto exit_mkdir_p;
            }
            if (p - tmp != len)
                *p = '/';
        }
    }

    ret = 0;

exit_mkdir_p:

    if (tmp)
        rt_free(tmp);

    return ret;
}

/* Initialize the filesystem */
int filesystem_init(void)
{
    // ramdisk as root
    if (rt_device_find(RAMDISK_NAME))
    {
        // format the ramdisk
        dfs_mkfs("elm", RAMDISK_NAME);

        /* mount ramdisk0 as root directory */
        if (dfs_mount(RAMDISK_NAME, "/", "elm", 0, RT_NULL) == 0)
        {
            LOG_I("ramdisk mounted on \"/\".");

            /* now you can create dir dynamically. */
            mkdir_p("/mnt", 0x777);
            mkdir_p("/cache", 0x777);
            mkdir_p("/download", 0x777);
#if defined(RT_USBH_MSTORAGE) && defined(UDISK_MOUNTPOINT)
            mkdir_p(UDISK_MOUNTPOINT, 0x777);
#endif
        }
        else
        {
            LOG_E("root folder creation failed!\n");
        }
        return RT_EOK;
    }
    LOG_E("cannot find %s device", RAMDISK_NAME);
    return -RT_ERROR;
}
INIT_ENV_EXPORT(filesystem_init);

extern rt_err_t ramdisk_init(const char *dev_name, rt_uint8_t *disk_addr, rt_size_t block_size, rt_size_t num_block);
int ramdisk_device_init(void)
{
    /* Create a 16MB RAMDISK */
    return (int)ramdisk_init(RAMDISK_NAME, NULL, 512, 2048 * 4);
}
INIT_DEVICE_EXPORT(ramdisk_device_init);
#endif

#if defined(BOARD_USING_STORAGE_SPIFLASH)
int mnt_init_spiflash0(void)
{
#if defined(PKG_USING_FAL)
    extern int fal_init_check(void);
    if (!fal_init_check())
        fal_init();
#endif
    struct rt_device *psNorFlash = fal_blk_device_create(PARTITION_NAME_FILESYSTEM);
    if (!psNorFlash)
    {
        rt_kprintf("Failed to create block device for %s.\n", PARTITION_NAME_FILESYSTEM);
        goto exit_mnt_init_spiflash0;
    }
    else if (mkdir(MOUNT_POINT_SPIFLASH0, 0x777) < 0)
    {
        rt_kprintf("Failed to make folder for %s.\n", MOUNT_POINT_SPIFLASH0);
        goto exit_mnt_init_spiflash0;
    }
    else if (dfs_mount(psNorFlash->parent.name, MOUNT_POINT_SPIFLASH0, "elm", 0, 0) != 0)
    {
        rt_kprintf("Failed to mount elm on %s.\n", MOUNT_POINT_SPIFLASH0);
        rt_kprintf("Try to execute 'mkfs -t elm %s' first, then reboot.\n", PARTITION_NAME_FILESYSTEM);
        goto exit_mnt_init_spiflash0;
    }
    rt_kprintf("mount %s with elmfat type: ok\n", PARTITION_NAME_FILESYSTEM);

exit_mnt_init_spiflash0:

    return 0;
}
INIT_ENV_EXPORT(mnt_init_spiflash0);
#endif

