#include "linux/cleanup.h"
#include "linux/gfp_types.h"
#include "linux/kern_levels.h"
#include "linux/mutex.h"
#include "linux/stddef.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "r04flash.h"

#define DEVICE_NAME "r04flash"
#define DRIVER "r04flash_driver"

#define R04FLASH_CSR_BAR_NO 0
#define R04FLASH_CSR_BAR_MASK (1 << R04FLASH_CSR_BAR_NO)

#define R04FLASH_DATA_BAR_NO 2
#define R04FLASH_DATA_BAR_MASK (1 << R04FLASH_DATA_BAR_NO)

#define DATA_OFFSET 8

static struct pci_device_id r04flash_id_table[] = {
	{ PCI_DEVICE(R04FLASH_VENDOR_ID, R04FLASH_PRODUCT_ID) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, r04flash_id_table);

static int r04flash_probe(struct pci_dev *pdev,
			  const struct pci_device_id *ent);
static void r04flash_remove(struct pci_dev *pdev);

static struct pci_driver r04flash = { .name = DRIVER,
				      .id_table = r04flash_id_table,
				      .probe = r04flash_probe,
				      .remove = r04flash_remove };

int create_char_devs(struct r04flash_data *drv);
int destroy_char_devs(void);

static int r04flash_open(struct inode *inode, struct file *file);
static int r04flash_release(struct inode *inode, struct file *file);
static long r04flash_ioctl(struct file *file, unsigned int cmd, u64 arg);
static ssize_t r04flash_read(struct file *file, char __user *buf, size_t count,
			     loff_t *offset);
static ssize_t r04flash_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *offset);

static const struct file_operations r04flash_fops = {
	.owner = THIS_MODULE,
	.open = r04flash_open,
	.release = r04flash_release,
	.unlocked_ioctl = (void *)r04flash_ioctl,
	.read = r04flash_read,
	.write = r04flash_write
};

static int dev_major = 0;
static struct class *r04flashclass = NULL;
static struct r04flash_data *r04flash_priv = NULL;
static struct r04flash_global_data r04flash_sync;

static ssize_t disk_size_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", ioread32(&r04flash_priv->csr->disk_size));
}

static ssize_t rd_addr_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "0x%llx\n", r04flash_priv->rd_addr);
}

static ssize_t rd_addr_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u64 new_value;

	if (kstrtou64(buf, 0, &new_value) != 0)
		return -EINVAL;

	r04flash_priv->rd_addr = new_value;
	printk(KERN_INFO "r04flash: set read addr to 0x%llx\n", new_value);
	return count;
}

static ssize_t rd_size_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%d\n", r04flash_priv->rd_max_size);
}

static ssize_t rd_size_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u32 new_value;

	if (kstrtou32(buf, 0, &new_value) != 0)
		return -EINVAL;

	if (new_value == 0 || new_value > WIN_SIZE)
		new_value = WIN_SIZE;
	r04flash_priv->rd_max_size = new_value;

	printk(KERN_INFO "r04flash: set max read size to %d\n", new_value);
	return count;
}

static ssize_t rd_timeout_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", r04flash_priv->rd_timeout);
}

static ssize_t rd_timeout_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 new_value;

	if (kstrtou32(buf, 0, &new_value) != 0)
		return -EINVAL;

	if (new_value == 0)
		new_value = R04FLASH_DEFAULT_TIMEOUT_U;

	r04flash_priv->rd_timeout = new_value;

	printk(KERN_INFO "r04flash: set read tmeout to %d\n", new_value);
	return count;
}

static ssize_t wr_addr_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "0x%llx\n", r04flash_priv->wr_addr);
}

static ssize_t wr_addr_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u64 new_value;

	if (kstrtou64(buf, 0, &new_value) != 0)
		return -EINVAL;

	r04flash_priv->wr_addr = new_value;
	printk(KERN_INFO "r04flash: set write address to 0x%llx\n", new_value);
	return count;
}

static ssize_t wr_size_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%d\n", ioread32(&r04flash_priv->wr_max_size));
}

static ssize_t wr_size_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u32 new_value;

	if (kstrtou32(buf, 0, &new_value) != 0)
		return -EINVAL;

	if (new_value == 0 || new_value > WIN_SIZE)
		new_value = WIN_SIZE;

	r04flash_priv->wr_max_size = new_value;

	printk(KERN_INFO "r04flash: set maximum write size to %d\n", new_value);
	return count;
}

static ssize_t wr_timeout_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", r04flash_priv->wr_timeout);
}

static ssize_t wr_timeout_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 new_value;

	if (kstrtou32(buf, 0, &new_value) != 0)
		return -EINVAL;

	if (new_value == 0)
		new_value = R04FLASH_DEFAULT_TIMEOUT_U;

	r04flash_priv->wr_timeout = new_value;

	printk(KERN_INFO "r04flash: set write timeout to %d\n", new_value);
	return count;
}

static DEVICE_ATTR(disk_size, 0444, disk_size_show, NULL);
static DEVICE_ATTR(rd_addr, 0664, rd_addr_show, rd_addr_store);
static DEVICE_ATTR(wr_addr, 0664, wr_addr_show, wr_addr_store);
static DEVICE_ATTR(rd_timeout, 0664, rd_timeout_show, rd_timeout_store);
static DEVICE_ATTR(wr_timeout, 0664, wr_timeout_show, wr_timeout_store);
static DEVICE_ATTR(rd_max_size, 0664, rd_size_show, rd_size_store);
static DEVICE_ATTR(wr_max_size, 0664, wr_size_show, wr_size_store);

#define CREATE_SYSFS_ATTR(_cls, _attr)                                       \
	err = device_create_file(_cls, &dev_attr_##_attr);                   \
	if (err < 0)                                                         \
		printk(KERN_ERR " " DRIVER                                   \
				": Failed to create sysfs attribute " #_attr \
				"\n");

int create_char_devs(struct r04flash_data *drv)
{
	int err;
	dev_t dev;

	err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);

	dev_major = MAJOR(dev);

	r04flash_priv = drv;

	// инициализируем character device
	cdev_init(&r04flash_priv->cdev, &r04flash_fops);

	r04flash_priv->cdev.owner = THIS_MODULE;
	cdev_add(&r04flash_priv->cdev, MKDEV(dev_major, 0), 1);

	// регистрируем sysfs класс
	r04flashclass = class_create(DEVICE_NAME);

	r04flash_priv->r04flash = device_create(
		r04flashclass, NULL, MKDEV(dev_major, 0), NULL, DEVICE_NAME);

	// Создаем sysfs атрибут
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, disk_size);
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, rd_addr);
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, wr_addr);
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, rd_max_size);
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, wr_max_size);
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, rd_timeout);
	CREATE_SYSFS_ATTR(r04flash_priv->r04flash, wr_timeout);

	r04flash_priv->rd_timeout = R04FLASH_DEFAULT_TIMEOUT_U;
	r04flash_priv->wr_timeout = R04FLASH_DEFAULT_TIMEOUT_U;
	r04flash_priv->rd_addr = 0;
	r04flash_priv->wr_addr = 0;
	r04flash_priv->rd_max_size = WIN_SIZE;
	r04flash_priv->wr_max_size = WIN_SIZE;

	return 0;
}

static int r04flash_open(struct inode *inode, struct file *file)
{
	file->private_data =
		kmemdup(r04flash_priv, sizeof(*r04flash_priv), GFP_KERNEL);
	if (!file->private_data)
		return -ENOMEM;
	return 0;
}

static irqreturn_t r04flash_irq(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	if (get_pcie_bar0_rd_status_comp(r04flash_priv->csr)) {
		unset_pcie_bar0_rd_status_comp(r04flash_priv->csr);
		complete(&r04flash_priv->read_complete);
		ret = IRQ_HANDLED;
		printk(KERN_INFO "r04flash: got read complete IRQ\n");
	}
	if (get_pcie_bar0_wr_status_comp(r04flash_priv->csr)) {
		unset_pcie_bar0_wr_status_comp(r04flash_priv->csr);
		complete(&r04flash_priv->write_complete);
		ret = IRQ_HANDLED;
		printk(KERN_INFO "r04flash: got write complete IRQ\n");
	}

	return ret;
}

static ssize_t r04flash_read(struct file *file, char __user *buf, size_t count,
			     loff_t *offset)
{
	struct r04flash_data *dev = file->private_data;
	int timeout, ret = 0, err;
	u64 addr = dev->rd_addr;
	u32 size;

	printk(KERN_INFO "r04flash: read(addr=0x%llx, size=0x%lx)", addr,
	       count);

	while (count) {
		err = mutex_lock_interruptible(&r04flash_sync.read_lock);
		if (err)
			return err;

		size = count < dev->rd_max_size ? count : dev->rd_max_size;
		printk(KERN_INFO "r04flash: read_chuck(addr=0x%llx, size=0x%x)",
		       addr, size);

		reinit_completion(&r04flash_priv->read_complete);

		iowrite32(size, &dev->csr->rd_desc.size);
		iowrite32(addr >> 32, &dev->csr->rd_desc.addr_high);
		iowrite32(addr, &dev->csr->rd_desc.addr_low);

		set_pcie_bar0_rd_ctrl_start(dev->csr);

		timeout = wait_for_completion_interruptible_timeout(
			&r04flash_priv->read_complete, dev->rd_timeout);
		if (timeout == 0) {
			ret = -ETIMEDOUT;
			goto err;
		} else if (timeout < 0) {
			ret = -EFAULT;
			goto err;
		}

		memcpy_fromio(r04flash_sync.rd_data_buf, dev->data->rd_data,
			      size);

		if (copy_to_user(buf, r04flash_sync.rd_data_buf, size)) {
			ret = -EFAULT;
			goto err;
		}

		if (get_pcie_bar0_rd_status_addr_error(dev->csr)) {
			ret = R04_ADDRINVAL;
			goto err;
		}
		if (get_pcie_bar0_rd_status_size_error(dev->csr)) {
			ret = R04_SIZEINVAL;
			goto err;
		}

		ret += size;
		count -= size;
		addr += size;
		buf += size;
		mutex_unlock(&r04flash_sync.read_lock);
	}

	return ret;
err:
	mutex_unlock(&r04flash_sync.read_lock);
	return err;
}

static ssize_t r04flash_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *offset)
{
	struct r04flash_data *dev = file->private_data;
	int timeout, ret = 0, err;
	u64 addr = dev->wr_addr;
	u32 size;

	printk(KERN_INFO "r04flash: write(addr=0x%llx, size=0x%lx)", addr,
	       count);

	while (count) {
		err = mutex_lock_interruptible(&r04flash_sync.write_lock);
		if (err)
			return err;

		size = count < dev->wr_max_size ? count : dev->wr_max_size;
		printk(KERN_INFO
		       "r04flash: write_chuck(addr=0x%llx, size=0x%x)",
		       addr, size);

		reinit_completion(&r04flash_priv->write_complete);

		iowrite32(size, &dev->csr->wr_desc.size);
		iowrite32(addr >> 32, &dev->csr->wr_desc.addr_high);
		iowrite32(addr, &dev->csr->wr_desc.addr_low);

		if (copy_from_user(r04flash_sync.wr_data_buf, buf, size)) {
			ret = -EFAULT;
			goto err;
		}

		memcpy_toio(dev->data->wr_data, r04flash_sync.wr_data_buf,
			    size);

		set_pcie_bar0_wr_ctrl_start(dev->csr);

		timeout = wait_for_completion_interruptible_timeout(
			&r04flash_priv->write_complete, dev->wr_timeout);
		if (timeout == 0) {
			ret = -ETIMEDOUT;
			goto err;
		} else if (timeout < 0) {
			ret = -EFAULT;
			goto err;
		}

		if (get_pcie_bar0_wr_status_addr_error(dev->csr)) {
			ret = R04_ADDRINVAL;
			goto err;
		}
		if (get_pcie_bar0_wr_status_size_error(dev->csr)) {
			ret = R04_SIZEINVAL;
			goto err;
		}

		ret += size;
		count -= size;
		addr += size;
		buf += size;
		mutex_unlock(&r04flash_sync.write_lock);
	}

	return ret;
err:
	mutex_unlock(&r04flash_sync.write_lock);
	return ret;
}

static long r04flash_ioctl(struct file *file, unsigned int cmd, u64 arg)
{
	struct r04flash_data *dev = file->private_data;
	switch (cmd) {
	case R04FLASH_IOCTL_SET_RD_ADDR:
		dev->rd_addr = arg;
		break;
	case R04FLASH_IOCTL_SET_RD_SIZE:
		dev->rd_max_size = arg;
		break;
	case R04FLASH_IOCTL_SET_RD_TIMEOUT:
		dev->rd_timeout = arg;
		break;
	case R04FLASH_IOCTL_SET_WR_ADDR:
		dev->wr_addr = arg;
		break;
	case R04FLASH_IOCTL_SET_WR_SIZE:
		dev->wr_max_size = arg;
		break;
	case R04FLASH_IOCTL_SET_WR_TIMEOUT:
		dev->wr_timeout = arg;
		break;
	default:
		printk(KERN_INFO
		       "r04flash: invalid ioctl cmd=0x%x, arg=0x%llx\n",
		       cmd, arg);
	}
	return 0;
}

static int r04flash_release(struct inode *inode, struct file *file)
{
	struct r04flash_data *priv = file->private_data;

	kfree(priv);

	file->private_data = NULL;

	return 0;
}

int destroy_char_devs(void)
{
	// Удаляем sysfs атрибут
	device_remove_file(r04flash_priv->r04flash, &dev_attr_disk_size);
	device_remove_file(r04flash_priv->r04flash, &dev_attr_rd_addr);
	device_remove_file(r04flash_priv->r04flash, &dev_attr_rd_timeout);
	device_remove_file(r04flash_priv->r04flash, &dev_attr_rd_max_size);
	device_remove_file(r04flash_priv->r04flash, &dev_attr_wr_addr);
	device_remove_file(r04flash_priv->r04flash, &dev_attr_wr_timeout);
	device_remove_file(r04flash_priv->r04flash, &dev_attr_wr_max_size);

	device_destroy(r04flashclass, MKDEV(dev_major, 0));

	class_unregister(r04flashclass);
	class_destroy(r04flashclass);
	unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

	return 0;
}

static int __init r04flash_driver_init(void)
{
	return pci_register_driver(&r04flash);
}

static void __exit r04flash_driver_exit(void)
{
	pci_unregister_driver(&r04flash);
}

static int r04flash_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int bar, err, irq;
	u16 vendor, device;
	unsigned long csr_bar_start, csr_bar_len;
	unsigned long data_bar_start, data_bar_len;
	struct r04flash_data *dev = NULL;
	__iomem void *csr_hwmem;
	__iomem void *data_hwmem;
	u8 *rd_data = NULL;
	u8 *wr_data = NULL;

	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

	printk(KERN_INFO "Device vid: 0x%X  pid: 0x%X\n", vendor, device);

	bar = pci_select_bars(pdev, IORESOURCE_MEM);
	if (!(bar & R04FLASH_CSR_BAR_MASK)) {
		dev_err(&pdev->dev, "Hardware does not privide bar for CSR\n");
		return -ENODEV;
	}

	if (!(bar & R04FLASH_DATA_BAR_MASK)) {
		dev_err(&pdev->dev,
			"Hardware does not privide bar for trantision windows\n");
		return -ENODEV;
	}

	err = pci_enable_device_mem(pdev);
	if (err)
		return err;

	err = pci_request_region(pdev, bar, DRIVER);
	if (err) {
		dev_err(&pdev->dev, "Failed to request region for bars\n");
		goto err_disable_device;
	}

	if (!pci_msi_enabled()) {
		dev_info(&pdev->dev, "MSI disabled system-wide\n");
		err = -ENOTSUPP;
		goto err_disable_region;
	}

	err = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to allocate IRQ vectors\n");
		goto err_disable_region;
	}

	irq = pci_irq_vector(pdev, 0);

	// Регистрируем обработчик прерывания
	err = request_irq(irq, r04flash_irq, 0, "r04flash", pdev);
	if (err) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		goto err_free_irq;
	}

	csr_bar_start = pci_resource_start(pdev, R04FLASH_CSR_BAR_NO);
	csr_bar_len = pci_resource_len(pdev, R04FLASH_CSR_BAR_NO);
	data_bar_start = pci_resource_start(pdev, R04FLASH_DATA_BAR_NO);
	data_bar_len = pci_resource_len(pdev, R04FLASH_DATA_BAR_NO);

	dev = kzalloc(sizeof(struct r04flash_data), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto err_disable_irq;
	}

	rd_data = kzalloc(WIN_SIZE, GFP_KERNEL);
	if (!rd_data) {
		err = -ENOMEM;
		goto err_free_dev;
	}

	wr_data = kzalloc(WIN_SIZE, GFP_KERNEL);
	if (!wr_data) {
		err = -ENOMEM;
		goto err_free_rd_data;
	}

	csr_hwmem = ioremap(csr_bar_start, csr_bar_len);
	if (!csr_hwmem) {
		dev_err(&pdev->dev, "Failed to map csr bar\n");
		err = -EIO;
		goto err_free_wr_data;
	}
	dev_info(&pdev->dev, "R04FLASH mapped resource 0x%lx to 0x%p\n",
		 csr_bar_start, csr_hwmem);

	data_hwmem = ioremap(data_bar_start, data_bar_len);
	if (!data_hwmem) {
		dev_err(&pdev->dev, "Failed to map data bar\n");
		err = -EIO;
		goto err_unmap_csr;
	}
	dev_info(&pdev->dev, "R04FLASH mapped resource 0x%lx to 0x%p\n",
		 data_bar_start, data_hwmem);

	dev->irq = irq;

	dev->csr = csr_hwmem;
	dev->data = data_hwmem;
	r04flash_sync.rd_data_buf = rd_data;
	r04flash_sync.wr_data_buf = wr_data;

	create_char_devs(dev);

	init_completion(&dev->read_complete);
	init_completion(&dev->write_complete);

	mutex_init(&r04flash_sync.read_lock);
	mutex_init(&r04flash_sync.write_lock);

	pci_set_drvdata(pdev, dev);

	dev_info(&pdev->dev, "R04FLASH probe success\n");

	return 0;
	// err_unmap_data:
	pci_iounmap(pdev, data_hwmem);
err_unmap_csr:
	pci_iounmap(pdev, csr_hwmem);
err_free_wr_data:
	kfree(wr_data);
err_free_rd_data:
	kfree(rd_data);
err_free_dev:
	kfree(dev);
err_disable_irq:
	pci_free_irq_vectors(pdev);
err_free_irq:
	free_irq(irq, pdev);
err_disable_region:
	pci_release_region(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
err_disable_device:
	pci_disable_device(pdev);
	return err;
}

static void r04flash_remove(struct pci_dev *pdev)
{
	struct r04flash_data *dev = pci_get_drvdata(pdev);

	destroy_char_devs();

	if (dev->irq)
		free_irq(dev->irq, pdev);

	pci_free_irq_vectors(pdev);

	if (dev) {
		if (dev->csr)
			pci_iounmap(pdev, dev->csr);
		if (dev->data)
			pci_iounmap(pdev, dev->data);
		kfree(dev);
	}

	if (r04flash_sync.rd_data_buf)
		kfree(r04flash_sync.rd_data_buf);
	if (r04flash_sync.wr_data_buf)
		kfree(r04flash_sync.wr_data_buf);

	mutex_destroy(&r04flash_sync.read_lock);
	mutex_destroy(&r04flash_sync.write_lock);

	pci_release_region(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	pci_disable_device(pdev);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Klimov Roman <klimov.roma04@yandex.ru>");
MODULE_DESCRIPTION("Driver for a QEMU PCI-r04flash device");
MODULE_VERSION("0.1");

module_init(r04flash_driver_init);
module_exit(r04flash_driver_exit);
