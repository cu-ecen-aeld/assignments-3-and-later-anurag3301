/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");
struct aesd_dev aesd_device;

static void free_all_entries(struct aesd_circular_buffer *buf)
{
    uint8_t idx;
    struct aesd_buffer_entry *entry;

    AESD_CIRCULAR_BUFFER_FOREACH(entry, buf, idx) {
        if (entry->buffptr) {
            kfree((void *)entry->buffptr);
            entry->buffptr = NULL;
            entry->size = 0;
        }
    }
}

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    PDEBUG("open");

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; // for other methods

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    // nothing specific to release per-file in this assignment
    return 0;
}

/*
 * Read returns data spanning the most recent 10 newline-terminated writes,
 * in order of receipt, respecting *f_pos and count.
 */
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = 0;
    struct aesd_buffer_entry *entry;
    size_t entry_offset = 0;
    size_t bytes_avail, to_copy;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (!dev || !buf)
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(
        &dev->circbuf, *f_pos, &entry_offset);

    if (!entry) {
        // No more data at this f_pos => EOF
        retval = 0;
        goto out_unlock;
    }

    bytes_avail = entry->size - entry_offset;
    to_copy = (count < bytes_avail) ? count : bytes_avail;

    if (copy_to_user(buf, entry->buffptr + entry_offset, to_copy)) {
        retval = -EFAULT;
        goto out_unlock;
    }

    *f_pos += to_copy;
    retval = to_copy;

out_unlock:
    mutex_unlock(&dev->lock);
    return retval;
}

/*
 * Write accumulates bytes into dev->working until a '\n' is seen.
 * Each completed command (ending in '\n') is pushed as one entry
 * into the circular buffer. Only last 10 are kept; older freed.
 *
 * We also handle the case where a single write contains multiple
 * newline-terminated commands by splitting them.
 */
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    char *kbuf = NULL;
    size_t copied = 0;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    if (!dev || !buf)
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        retval = -ENOMEM;
        goto out_unlock;
    }

    if (copy_from_user(kbuf, buf, count)) {
        retval = -EFAULT;
        goto out_free;
    }

    /*
     * Process possibly multiple commands within this single write:
     * Append into dev->working, and when we encounter '\n',
     * push current working buffer as a completed entry.
     */
    {
        size_t start = 0;
        while (start < count) {
            char *nl = memchr(kbuf + start, '\n', count - start);
            size_t chunk_len = nl ? (size_t)(nl - (kbuf + start) + 1)
                                  : (count - start);

            // Grow working buffer to hold current chunk
            {
                size_t new_size = dev->working.size + chunk_len;
                char *new_ptr = krealloc((void *)dev->working.buffptr,
                                         new_size, GFP_KERNEL);
                if (!new_ptr) {
                    retval = -ENOMEM;
                    goto out_free;
                }
                memcpy(new_ptr + dev->working.size, kbuf + start, chunk_len);
                dev->working.buffptr = new_ptr;
                dev->working.size = new_size;
            }

            start += chunk_len;
            copied += chunk_len;


            if (nl) {
                // If buffer is full, we will overwrite the oldest entry
                if (dev->circbuf.full) {
                    struct aesd_buffer_entry *oldest =
                        &dev->circbuf.entry[dev->circbuf.out_offs];

                    if (oldest->buffptr) {
                        kfree(oldest->buffptr);
                    }
                }

                // Add the new entry
                aesd_circular_buffer_add_entry(&dev->circbuf, &dev->working);

                // Reset working for the next potential command
                dev->working.buffptr = NULL;
                dev->working.size = 0;
            }
        }
    }

    retval = copied; // report bytes consumed from this write

out_free:
    kfree(kbuf);
out_unlock:
    mutex_unlock(&dev->lock);
    return retval;
}

struct file_operations aesd_fops = {
    .owner   = THIS_MODULE,
    .read    = aesd_read,
    .write   = aesd_write,
    .open    = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    // AESD-specific init
    aesd_circular_buffer_init(&aesd_device.circbuf);
    aesd_device.working.buffptr = NULL;
    aesd_device.working.size = 0;
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);
    if (result)
        unregister_chrdev_region(dev, 1);

    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Free working buffer (unterminated command, if any)
    if (aesd_device.working.buffptr)
        kfree((void *)aesd_device.working.buffptr);

    // Free all stored entries in the ring
    free_all_entries(&aesd_device.circbuf);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
