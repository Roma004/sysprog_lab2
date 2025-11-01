#pragma once
#include <stdint.h>

#define KiB           1024
#define WIN_SIZE      4 * KiB
#define FIELD_SIZE    64
#define POOLING_DELAY 200

#define ALIGNED(_size) __attribute__((aligned(_size)))

#define __field         ALIGNED(FIELD_SIZE)
#define __window(_name) ALIGNED(WIN_SIZE) uint8_t _name[WIN_SIZE]

struct pcie_desc {
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t size;
};

struct pcie_bar2 {
    // размер диска
    __field uint32_t disk_size;

    // регистр с прерываниями
    // - rd_comp
    // - wr_comp
    __field uint32_t irq;

    __field struct {
        // регистр статуса чтения
        // - start
        uint8_t rd_csr;

        // регистр ошибки чтения
        // - addr_error
        // - size_error
        uint8_t rd_err;

        // регистр статуса записи
        // - start
        uint8_t wr_csr;

        // регистр ошибки записи
        // - addr_error
        // - size_error
        uint8_t wr_err;
    };

    // дескриптор чтения
    __field struct pcie_desc rd_desc;

    // дескриптор записи
    __field struct pcie_desc wr_desc;

    __window(rd_data);
    __window(wr_data);
};

/*
 * -------------------------------  DEVICE  ------------------------------------
 * Алгоритм работы процесса произведения опреаций чтения:
 * - Ожидание бита rd_start в csr.
 * - Сброс прерывания rd_comp
 * - Сброс регистров: rd_err
 * - Проверка дескриптора
 *   - Если в дескрипторе не валиден адрес, установить addr_error бит в rd_err
 *   - Если в дескрипторе не валиден размер, установить size_error бит в rd_err
 *   - Если выполнено хотя бы что-то из вышеперечисленного, установить
 *     прерывание rd_comp и закончить алгоритм
 * - Блокировка данных по заданному адресу на заданный размер
 * - Копиросание данных из памяти в трансляционное пространство чтения
 * - Разблокировка адреса
 * - Сброс бита rd_start
 * - Установить прерывание rd_comp
 *
 * Алгоритм работы процесса произведения опреаций записи:
 * - Ожидание бита wr_start в csr.
 * - Сброс прерывания wr_comp
 * - Сброс регистров: wr_err
 * - Проверка дескриптора
 *   - Если в дескрипторе не валиден адрес, установить addr_error бит в wr_err
 *   - Если в дескрипторе не валиден размер, установить size_error бит в wr_err
 *   - Если выполнено хотя бы что-то из вышеперечисленного, установить
 *     прерывание wr_comp и закончить алгоритм
 * - Блокировка данных по заданному адресу на заданный размер
 * - Копирование данных из трансляционного пространства записи в память
 * - Разблокировка адреса.
 * - Сброс бита wr_start
 * - Установить прерывание wr_comp
 *
 * Алгоритм блокировки адреса памяти для чтения (симетрично для записи):
 * - проверить, пересекается ли диапазон с тем, что заблокирован для записи
 *   - если не пересекается (или не установлен), установить блокировку чтения
 *   - если пересекается, установить эту блокировку при снятии блокировки чтения
 *
 * Алгоритм снятия блокировки адреса памяти для чтения (симетрично для записи):
 * - если есть блокировака записи в очереди на установку, установить
 * - снять блокировку чтения
 *
 * --------------------------------  HOST  -------------------------------------
 * Порядок инициализации чтения:
 * 1. Проверка бита rd_start регистра csr
 * 2. Установка дескриптора чтения
 * 3. Ожидание прерывания rd_comp
 * 4. Проверка rd_err регистра:
 *    - если не пустой, возврат кода ошибки
 * 5. Извлечение данных из пространства чтения
 *
 * Порядок инициализации записи:
 * 1. Проверка бита wr_start регистра csr
 * 2. Установка дескриптора записи
 * 3. Ожидание прерывания wr_comp
 * 4. Проверка wr_err регистра:
 *    - если не пустой, возврат кода ошибки
 * 5. Извлечение данных из пространства записи
 *
 * */

#define PCIE_BAR2_IRQ_RD_COMP_OFST (0)
#define PCIE_BAR2_IRQ_RD_COMP_MASK (1 << PCIE_BAR2_IRQ_RD_COMP_OFST)

#define PCIE_BAR2_IRQ_WR_COMP_OFST (1)
#define PCIE_BAR2_IRQ_WR_COMP_MASK (1 << PCIE_BAR2_IRQ_WR_COMP_OFST)

#define PCIE_BAR2_RD_CSR_START_OFST (0)
#define PCIE_BAR2_RD_CSR_START_MASK (1 << PCIE_BAR2_RD_CSR_START_OFST)

#define PCIE_BAR2_RD_ERR_ADDR_ERROR_OFST (0)
#define PCIE_BAR2_RD_ERR_ADDR_ERROR_MASK (1 << PCIE_BAR2_RD_ERR_ADDR_ERROR_OFST)

#define PCIE_BAR2_RD_ERR_SIZE_ERROR_OFST (1)
#define PCIE_BAR2_RD_ERR_SIZE_ERROR_MASK (1 << PCIE_BAR2_RD_ERR_SIZE_ERROR_OFST)

#define PCIE_BAR2_WR_CSR_START_OFST (0)
#define PCIE_BAR2_WR_CSR_START_MASK (1 << PCIE_BAR2_WR_CSR_START_OFST)

#define PCIE_BAR2_WR_ERR_ADDR_ERROR_OFST (0)
#define PCIE_BAR2_WR_ERR_ADDR_ERROR_MASK (1 << PCIE_BAR2_WR_ERR_ADDR_ERROR_OFST)

#define PCIE_BAR2_WR_ERR_SIZE_ERROR_OFST (1)
#define PCIE_BAR2_WR_ERR_SIZE_ERROR_MASK (1 << PCIE_BAR2_WR_ERR_SIZE_ERROR_OFST)

static inline int get_pcie_bar2_irq_rd_comp(volatile struct pcie_bar2 *pcie) {
    return (pcie->irq & PCIE_BAR2_IRQ_RD_COMP_MASK)
        >> PCIE_BAR2_IRQ_RD_COMP_OFST;
}

static inline void set_pcie_bar2_irq_rd_comp(volatile struct pcie_bar2 *pcie) {
    pcie->irq |= PCIE_BAR2_IRQ_RD_COMP_MASK;
}

static inline void
unset_pcie_bar2_irq_rd_comp(volatile struct pcie_bar2 *pcie) {
    pcie->irq &= ~PCIE_BAR2_IRQ_RD_COMP_MASK;
}

static inline int get_pcie_bar2_irq_wr_comp(volatile struct pcie_bar2 *pcie) {
    return (pcie->irq & PCIE_BAR2_IRQ_WR_COMP_MASK)
        >> PCIE_BAR2_IRQ_WR_COMP_OFST;
}

static inline void set_pcie_bar2_irq_wr_comp(volatile struct pcie_bar2 *pcie) {
    pcie->irq |= PCIE_BAR2_IRQ_WR_COMP_MASK;
}

static inline void
unset_pcie_bar2_irq_wr_comp(volatile struct pcie_bar2 *pcie) {
    pcie->irq &= ~PCIE_BAR2_IRQ_WR_COMP_MASK;
}

static inline int get_pcie_bar2_rd_csr_start(volatile struct pcie_bar2 *pcie) {
    return (pcie->rd_csr & PCIE_BAR2_RD_CSR_START_MASK)
        >> PCIE_BAR2_RD_CSR_START_OFST;
}

static inline void set_pcie_bar2_rd_csr_start(volatile struct pcie_bar2 *pcie) {
    pcie->rd_csr |= PCIE_BAR2_RD_CSR_START_MASK;
}

static inline void
unset_pcie_bar2_rd_csr_start(volatile struct pcie_bar2 *pcie) {
    pcie->rd_csr &= ~PCIE_BAR2_RD_CSR_START_MASK;
}

static inline int
get_pcie_bar2_rd_err_addr_error(volatile struct pcie_bar2 *pcie) {
    return (pcie->rd_err & PCIE_BAR2_RD_ERR_ADDR_ERROR_MASK)
        >> PCIE_BAR2_RD_ERR_ADDR_ERROR_OFST;
}

static inline void
set_pcie_bar2_rd_err_addr_error(volatile struct pcie_bar2 *pcie) {
    pcie->rd_err |= PCIE_BAR2_RD_ERR_ADDR_ERROR_MASK;
}

static inline void
unset_pcie_bar2_rd_err_addr_error(volatile struct pcie_bar2 *pcie) {
    pcie->rd_err &= ~PCIE_BAR2_RD_ERR_ADDR_ERROR_MASK;
}

static inline int
get_pcie_bar2_rd_err_size_error(volatile struct pcie_bar2 *pcie) {
    return (pcie->rd_err & PCIE_BAR2_RD_ERR_SIZE_ERROR_MASK)
        >> PCIE_BAR2_RD_ERR_SIZE_ERROR_OFST;
}

static inline void
set_pcie_bar2_rd_err_size_error(volatile struct pcie_bar2 *pcie) {
    pcie->rd_err |= PCIE_BAR2_RD_ERR_SIZE_ERROR_MASK;
}

static inline void
unset_pcie_bar2_rd_err_size_error(volatile struct pcie_bar2 *pcie) {
    pcie->rd_err &= ~PCIE_BAR2_RD_ERR_SIZE_ERROR_MASK;
}

static inline int get_pcie_bar2_wr_csr_start(volatile struct pcie_bar2 *pcie) {
    return (pcie->wr_csr & PCIE_BAR2_WR_CSR_START_MASK)
        >> PCIE_BAR2_WR_CSR_START_OFST;
}

static inline void set_pcie_bar2_wr_csr_start(volatile struct pcie_bar2 *pcie) {
    pcie->wr_csr |= PCIE_BAR2_WR_CSR_START_MASK;
}

static inline void
unset_pcie_bar2_wr_csr_start(volatile struct pcie_bar2 *pcie) {
    pcie->wr_csr &= ~PCIE_BAR2_WR_CSR_START_MASK;
}

static inline int
get_pcie_bar2_wr_err_addr_error(volatile struct pcie_bar2 *pcie) {
    return (pcie->wr_err & PCIE_BAR2_WR_ERR_ADDR_ERROR_MASK)
        >> PCIE_BAR2_WR_ERR_ADDR_ERROR_OFST;
}

static inline void
set_pcie_bar2_wr_err_addr_error(volatile struct pcie_bar2 *pcie) {
    pcie->wr_err |= PCIE_BAR2_WR_ERR_ADDR_ERROR_MASK;
}

static inline void
unset_pcie_bar2_wr_err_addr_error(volatile struct pcie_bar2 *pcie) {
    pcie->wr_err &= ~PCIE_BAR2_WR_ERR_ADDR_ERROR_MASK;
}

static inline int
get_pcie_bar2_wr_err_size_error(volatile struct pcie_bar2 *pcie) {
    return (pcie->wr_err & PCIE_BAR2_WR_ERR_SIZE_ERROR_MASK)
        >> PCIE_BAR2_WR_ERR_SIZE_ERROR_OFST;
}

static inline void
set_pcie_bar2_wr_err_size_error(volatile struct pcie_bar2 *pcie) {
    pcie->wr_err |= PCIE_BAR2_WR_ERR_SIZE_ERROR_MASK;
}

static inline void
unset_pcie_bar2_wr_err_size_error(volatile struct pcie_bar2 *pcie) {
    pcie->wr_err &= ~PCIE_BAR2_WR_ERR_SIZE_ERROR_MASK;
}

