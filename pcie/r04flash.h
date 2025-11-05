#pragma once
#include "linux/completion.h"
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/compiler_attributes.h>
#include "asm-generic/iomap.h"

#define R04FLASH_VENDOR_ID 0x1B36
#define R04FLASH_PRODUCT_ID 0x0005

#define R04FLASH_IOCTL_SET_RD_ADDR    0x0001
#define R04FLASH_IOCTL_SET_RD_SIZE    0x0002
#define R04FLASH_IOCTL_SET_RD_TIMEOUT 0x0003

#define R04FLASH_IOCTL_SET_WR_ADDR    0x0101
#define R04FLASH_IOCTL_SET_WR_SIZE    0x0102
#define R04FLASH_IOCTL_SET_WR_TIMEOUT 0x0103

#define R04FLASH_DEFAULT_TIMEOUT_U 2000

#define WIN_SIZE 32 * 1024

#define __field __aligned(64)
#define __window(_name) __aligned(WIN_SIZE) u8 _name[WIN_SIZE]

struct pcie_desc {
	u32 addr_low;
	u32 addr_high;
	u32 size;
};

struct pcie_bar0 {
	// размер диска
	__field u32 disk_size;

	__field struct {
		// регистр статуса чтения
		// - start
		u8 rd_ctrl;

		// регистр ошибки чтения
		// - comp
		// - addr_error
		// - size_error
		u8 rd_status;

		// регистр статуса записи
		// - start
		u8 wr_ctrl;

		// регистр ошибки записи
		// - comp
		// - addr_error
		// - size_error
		u8 wr_status;
	};

	// дескриптор чтения
	__field struct pcie_desc rd_desc;

	// дескриптор записи
	__field struct pcie_desc wr_desc;
};

struct pcie_bar2 {
	__window(rd_data);
	__window(wr_data);
};

struct r04flash_global_data {
	struct mutex read_lock;
	struct mutex write_lock;

	u8 *rd_data_buf;
	u8 *wr_data_buf;
};

struct r04flash_data {
	struct cdev cdev;
	struct device *r04flash;

	__iomem struct pcie_bar0 *csr;
	__iomem struct pcie_bar2 *data;
	int irq;

	u32 rd_max_size;
	u32 wr_max_size;

	u64 rd_addr;
	u64 wr_addr;

	int rd_timeout;
	int wr_timeout;

	struct completion read_complete;
	struct completion write_complete;
};

enum r04flash_error {
	R04_ADDRINVAL = 1,
	R04_SIZEINVAL = 2,
};

#define PCIE_BAR0_RD_CTRL_START_OFST (0)
#define PCIE_BAR0_RD_CTRL_START_MASK (1 << PCIE_BAR0_RD_CTRL_START_OFST)

#define PCIE_BAR0_WR_CTRL_START_OFST (0)
#define PCIE_BAR0_WR_CTRL_START_MASK (1 << PCIE_BAR0_WR_CTRL_START_OFST)

#define PCIE_BAR0_RD_STATUS_COMP_OFST (0)
#define PCIE_BAR0_RD_STATUS_COMP_MASK (1 << PCIE_BAR0_RD_STATUS_COMP_OFST)

#define PCIE_BAR0_RD_STATUS_ADDR_ERROR_OFST (6)
#define PCIE_BAR0_RD_STATUS_ADDR_ERROR_MASK \
	(1 << PCIE_BAR0_RD_STATUS_ADDR_ERROR_OFST)

#define PCIE_BAR0_RD_STATUS_SIZE_ERROR_OFST (7)
#define PCIE_BAR0_RD_STATUS_SIZE_ERROR_MASK \
	(1 << PCIE_BAR0_RD_STATUS_SIZE_ERROR_OFST)

#define PCIE_BAR0_WR_STATUS_COMP_OFST (0)
#define PCIE_BAR0_WR_STATUS_COMP_MASK (1 << PCIE_BAR0_WR_STATUS_COMP_OFST)

#define PCIE_BAR0_WR_STATUS_ADDR_ERROR_OFST (6)
#define PCIE_BAR0_WR_STATUS_ADDR_ERROR_MASK \
	(1 << PCIE_BAR0_WR_STATUS_ADDR_ERROR_OFST)

#define PCIE_BAR0_WR_STATUS_SIZE_ERROR_OFST (7)
#define PCIE_BAR0_WR_STATUS_SIZE_ERROR_MASK \
	(1 << PCIE_BAR0_WR_STATUS_SIZE_ERROR_OFST)

static inline int get_pcie_bar0_rd_ctrl_start(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->rd_ctrl) & PCIE_BAR0_RD_CTRL_START_MASK) >>
	       PCIE_BAR0_RD_CTRL_START_OFST;
}

static inline void set_pcie_bar0_rd_ctrl_start(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_ctrl) | PCIE_BAR0_RD_CTRL_START_MASK;
	iowrite8(new_value, &pcie->rd_ctrl);
}

static inline void unset_pcie_bar0_rd_ctrl_start(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_ctrl) & ~PCIE_BAR0_RD_CTRL_START_MASK;
	iowrite8(new_value, &pcie->rd_ctrl);
}

static inline int get_pcie_bar0_wr_ctrl_start(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->wr_ctrl) & PCIE_BAR0_WR_CTRL_START_MASK) >>
	       PCIE_BAR0_WR_CTRL_START_OFST;
}

static inline void set_pcie_bar0_wr_ctrl_start(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_ctrl) | PCIE_BAR0_WR_CTRL_START_MASK;
	iowrite8(new_value, &pcie->wr_ctrl);
}

static inline void unset_pcie_bar0_wr_ctrl_start(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_ctrl) & ~PCIE_BAR0_WR_CTRL_START_MASK;
	iowrite8(new_value, &pcie->wr_ctrl);
}

static inline int get_pcie_bar0_rd_status_comp(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->rd_status) & PCIE_BAR0_RD_STATUS_COMP_MASK) >>
	       PCIE_BAR0_RD_STATUS_COMP_OFST;
}

static inline void set_pcie_bar0_rd_status_comp(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_status) |
			PCIE_BAR0_RD_STATUS_COMP_MASK;
	iowrite8(new_value, &pcie->rd_status);
}

static inline void
unset_pcie_bar0_rd_status_comp(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_status) &
			~PCIE_BAR0_RD_STATUS_COMP_MASK;
	iowrite8(new_value, &pcie->rd_status);
}

static inline int
get_pcie_bar0_rd_status_addr_error(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->rd_status) &
		PCIE_BAR0_RD_STATUS_ADDR_ERROR_MASK) >>
	       PCIE_BAR0_RD_STATUS_ADDR_ERROR_OFST;
}

static inline void
set_pcie_bar0_rd_status_addr_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_status) |
			PCIE_BAR0_RD_STATUS_ADDR_ERROR_MASK;
	iowrite8(new_value, &pcie->rd_status);
}

static inline void
unset_pcie_bar0_rd_status_addr_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_status) &
			~PCIE_BAR0_RD_STATUS_ADDR_ERROR_MASK;
	iowrite8(new_value, &pcie->rd_status);
}

static inline int
get_pcie_bar0_rd_status_size_error(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->rd_status) &
		PCIE_BAR0_RD_STATUS_SIZE_ERROR_MASK) >>
	       PCIE_BAR0_RD_STATUS_SIZE_ERROR_OFST;
}

static inline void
set_pcie_bar0_rd_status_size_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_status) |
			PCIE_BAR0_RD_STATUS_SIZE_ERROR_MASK;
	iowrite8(new_value, &pcie->rd_status);
}

static inline void
unset_pcie_bar0_rd_status_size_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->rd_status) &
			~PCIE_BAR0_RD_STATUS_SIZE_ERROR_MASK;
	iowrite8(new_value, &pcie->rd_status);
}

static inline int get_pcie_bar0_wr_status_comp(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->wr_status) & PCIE_BAR0_WR_STATUS_COMP_MASK) >>
	       PCIE_BAR0_WR_STATUS_COMP_OFST;
}

static inline void set_pcie_bar0_wr_status_comp(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_status) |
			PCIE_BAR0_WR_STATUS_COMP_MASK;
	iowrite8(new_value, &pcie->wr_status);
}

static inline void
unset_pcie_bar0_wr_status_comp(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_status) &
			~PCIE_BAR0_WR_STATUS_COMP_MASK;
	iowrite8(new_value, &pcie->wr_status);
}

static inline int
get_pcie_bar0_wr_status_addr_error(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->wr_status) &
		PCIE_BAR0_WR_STATUS_ADDR_ERROR_MASK) >>
	       PCIE_BAR0_WR_STATUS_ADDR_ERROR_OFST;
}

static inline void
set_pcie_bar0_wr_status_addr_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_status) |
			PCIE_BAR0_WR_STATUS_ADDR_ERROR_MASK;
	iowrite8(new_value, &pcie->wr_status);
}

static inline void
unset_pcie_bar0_wr_status_addr_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_status) &
			~PCIE_BAR0_WR_STATUS_ADDR_ERROR_MASK;
	iowrite8(new_value, &pcie->wr_status);
}

static inline int
get_pcie_bar0_wr_status_size_error(__iomem struct pcie_bar0 *pcie)
{
	return (ioread8(&pcie->wr_status) &
		PCIE_BAR0_WR_STATUS_SIZE_ERROR_MASK) >>
	       PCIE_BAR0_WR_STATUS_SIZE_ERROR_OFST;
}

static inline void
set_pcie_bar0_wr_status_size_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_status) |
			PCIE_BAR0_WR_STATUS_SIZE_ERROR_MASK;
	iowrite8(new_value, &pcie->wr_status);
}

static inline void
unset_pcie_bar0_wr_status_size_error(__iomem struct pcie_bar0 *pcie)
{
	int new_value = ioread8(&pcie->wr_status) &
			~PCIE_BAR0_WR_STATUS_SIZE_ERROR_MASK;
	iowrite8(new_value, &pcie->wr_status);
}

