#include <linux/mutex.h>
#include "../common/chip_ana_reg.h"
#include "../common/chip_pmu_reg.h"
#include "../common/chip_intf_reg.h"
#include "../common/wifi_intf_addr.h"
#include "../common/wifi_top_addr.h"
#include "../common/wifi_sdio_cfg_addr.h"
#include "share_mem_map.h"
#include "aml_defs.h"
#include "sdio_common.h"
#include "sg_common.h"
uint8_t *g_mmc_misc;
struct aml_hwif_sdio g_hwif_sdio;
struct aml_hwif_sdio g_hwif_rx_sdio;
struct aml_hif_sdio_ops g_hif_sdio_ops;

unsigned char g_sdio_wifi_bt_alive;
unsigned char g_sdio_driver_insmoded;
unsigned char g_sdio_after_porbe;
unsigned char g_wifi_in_insmod;
unsigned char *g_func_kmalloc_buf = NULL;

static unsigned int tx_buffer_base_addr;
static unsigned int rx_buffer_base_addr;

static DEFINE_MUTEX(wifi_bt_sdio_mutex);
static DEFINE_MUTEX(wifi_ipc_mutex);

#define AML_BT_WIFI_MUTEX_ON() do {\
    mutex_lock(&wifi_bt_sdio_mutex);\
} while (0)

#define AML_BT_WIFI_MUTEX_OFF() do {\
    mutex_unlock(&wifi_bt_sdio_mutex);\
} while (0)

#define AML_WIFI_IPC_MUTEX_ON() do {\
    mutex_lock(&wifi_ipc_mutex);\
} while (0)

#define AML_WIFI_IPC_MUTEX_OFF() do {\
    mutex_unlock(&wifi_ipc_mutex);\
} while (0)

unsigned char (*host_wake_req)(void);
int (*host_suspend_req)(struct device *device);
int (*host_resume_req)(struct device *device);

struct sdio_func *aml_priv_to_func(int func_n)
{
    ASSERT(func_n >= 0 &&  func_n < SDIO_FUNCNUM_MAX);
    return g_hwif_sdio.sdio_func_if[func_n];
}

static int _aml_sdio_request_byte(unsigned char func_num,
    unsigned char write, unsigned int reg_addr, unsigned char *byte)
{
    int err_ret = 0;
    struct sdio_func * func = aml_priv_to_func(func_num);
    unsigned char *kmalloc_buf = NULL;
    unsigned char len = sizeof(unsigned char);

#if defined(DBG_PRINT_COST_TIME)
    struct timespec now, before;
    getnstimeofday(&before);
#endif /* End of DBG_PRINT_COST_TIME */

    if (!func) {
        printk("func is NULL!\n");
        return -1;
    }

    if (!byte) {
        printk("byte is NULL!\n");
        return -1;
    }

    ASSERT(func->num == func_num);

    AML_BT_WIFI_MUTEX_ON();
    kmalloc_buf =  (unsigned char *)ZMALLOC(len, "sdio_write", GFP_DMA);
    if (kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        AML_BT_WIFI_MUTEX_OFF();
        return SDIOH_API_RC_FAIL;
    }
    memcpy(kmalloc_buf, byte, len);

    /* Claim host controller */
    sdio_claim_host(func);

    if (write) {
        /* CMD52 Write */
        sdio_writeb(func, *kmalloc_buf, reg_addr, &err_ret);
    }
    else {
        /* CMD52 Read */
        *byte = sdio_readb(func, reg_addr, &err_ret);
    }

    /* Release host controller */
    sdio_release_host(func);

#if defined(DBG_PRINT_COST_TIME)
    getnstimeofday(&now);

    printk("[sdio byte]: len=1 cost=%lds %luus\n",
        now.tv_sec-before.tv_sec, now.tv_nsec/1000 - before.tv_nsec/1000);
#endif /* End of DBG_PRINT_COST_TIME */

    FREE(kmalloc_buf, "sdio_write");
    AML_BT_WIFI_MUTEX_OFF();
    return (err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL;
}


//cmd52, func 1, for self define domain
int aml_sdio_self_define_domain_write8(int addr, unsigned char data)
{
    int ret = 0;

    ret =  _aml_sdio_request_byte(SDIO_FUNC1, SDIO_WRITE, addr, &data);
    return ret;
}

//cmd52
unsigned char aml_sdio_self_define_domain_read8(int addr)
{
    unsigned char sramdata;

    _aml_sdio_request_byte(SDIO_FUNC1, SDIO_READ, addr, &sramdata);
    return sramdata;
}

//cmd53
static int _aml_sdio_request_buffer(unsigned char func_num,
    unsigned int fix_incr, unsigned char write, unsigned int addr, void *buf, unsigned int nbytes)
{
    int err_ret = 0;
    int align_nbytes = nbytes;
    struct sdio_func * func = aml_priv_to_func(func_num);
    bool fifo = (fix_incr == SDIO_OPMODE_FIXED);

    if (!func) {
        printk("func is NULL!\n");
        return -1;
    }

    ASSERT(fix_incr == SDIO_OPMODE_FIXED|| fix_incr == SDIO_OPMODE_INCREMENT);
    ASSERT(func->num == func_num);

    /* Claim host controller */
    sdio_claim_host(func);

    if (write && !fifo)
    {
        /* write, increment */
        align_nbytes = sdio_align_size(func, nbytes);
        err_ret = sdio_memcpy_toio(func, addr, buf, align_nbytes);
    }
    else if (write)
    {
        /* write, fifo */
        err_ret = sdio_writesb(func, addr, buf, align_nbytes);
    }
    else if (fifo)
    {
        /* read */
        err_ret = sdio_readsb(func, buf, addr, align_nbytes);
    }
    else
    {
        /* read */
        align_nbytes = sdio_align_size(func, nbytes);
        err_ret = sdio_memcpy_fromio(func, buf, addr, align_nbytes);
    }

    /* Release host controller */
    sdio_release_host(func);

    return (err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL;
}

//cmd53
int aml_sdio_bottom_write(unsigned char func_num, unsigned int addr, void *buf, size_t len, int incr_addr)
{
    void *kmalloc_buf;
    int result;

    ASSERT(func_num != SDIO_FUNC0);
    ASSERT(g_func_kmalloc_buf);

    if (host_wake_req != NULL) {
        if (host_wake_req() == 0) {
            ERROR_DEBUG_OUT("aml_sdio_bottom_write, host wake fail\n");
            return -1;
        }
    }

    AML_BT_WIFI_MUTEX_ON();
    kmalloc_buf = (unsigned char *)g_func_kmalloc_buf;
    memcpy(kmalloc_buf, buf, len);

    result = _aml_sdio_request_buffer(func_num, incr_addr, SDIO_WRITE, addr, kmalloc_buf, len);

    AML_BT_WIFI_MUTEX_OFF();
    return result;
}

int aml_sdio_bottom_read(unsigned char func_num, int addr, void *buf, size_t len, int incr_addr)
{
    unsigned char *kmalloc_buf = NULL;
    int result;
    int align_len = 0;
    unsigned char scat_use = func_num & (1 << 8);

    func_num &= 0xf;
    ASSERT(func_num != SDIO_FUNC0);
    ASSERT(g_func_kmalloc_buf);

    if (host_wake_req != NULL) {
        if (host_wake_req() == 0) {
            ERROR_DEBUG_OUT("aml_sdio_bottom_read, host wake fail\n");
            return -1;
        }
    }

    AML_BT_WIFI_MUTEX_ON();
    /* read block mode */
    if (func_num != SDIO_FUNC0) {
        if (incr_addr == SDIO_OPMODE_INCREMENT) {
            struct sdio_func * func = aml_priv_to_func(func_num);
            align_len = sdio_align_size(func, len);

            if ((func_num == SDIO_FUNC6) && !scat_use) {
                kmalloc_buf = (unsigned char *)g_func_kmalloc_buf;
            } else {
                kmalloc_buf = (unsigned char *)g_func_kmalloc_buf;
            }
        }
        else
            kmalloc_buf = (unsigned char *)g_func_kmalloc_buf;
    } else {
        kmalloc_buf = (unsigned char *)buf;
    }

    if (kmalloc_buf == NULL) {
        ERROR_DEBUG_OUT("kmalloc buf fail kmalloc_buf %x buf %x SDIO_FUNC %d\n",kmalloc_buf, buf,func_num);
        AML_BT_WIFI_MUTEX_OFF();
        return SDIOH_API_RC_FAIL;
    }

    result = _aml_sdio_request_buffer(func_num, incr_addr, SDIO_READ, addr, kmalloc_buf, len);

    if (kmalloc_buf != buf) {
        memcpy(buf, kmalloc_buf, len);
    }

    AML_BT_WIFI_MUTEX_OFF();
    return result;
}

//func 1, cmd52, self define domain
int aml_sdio_self_define_domain_write32(unsigned long sram_addr, unsigned long sramdata)
{
    return aml_sdio_bottom_write(SDIO_FUNC1, sram_addr&SDIO_ADDR_MASK,
        (unsigned char *)&sramdata,  sizeof(unsigned long), SDIO_OPMODE_INCREMENT);
}

unsigned long  aml_sdio_self_define_domain_read32(unsigned long sram_addr)
{
    unsigned long sramdata = 0;

    aml_sdio_bottom_read(SDIO_FUNC1, sram_addr&SDIO_ADDR_MASK, &sramdata, 4, SDIO_OPMODE_INCREMENT);
    return sramdata;
}

//func2, for random ram
void aml_sdio_random_word_write(unsigned int addr, unsigned int data)
{
    unsigned int len = sizeof(unsigned int);

    AML_WIFI_IPC_MUTEX_ON();

    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC2_BADDR_A, (unsigned long)(addr) & 0xfffe0000);
    aml_sdio_bottom_write(SDIO_FUNC2, (SYS_TYPE)addr & SDIO_ADDR_MASK,
        (unsigned char *)&data, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    AML_WIFI_IPC_MUTEX_OFF();
}

unsigned int aml_sdio_random_word_read(unsigned int addr)
{
    unsigned int regdata = 0;
    unsigned int len = sizeof(unsigned int);

    AML_WIFI_IPC_MUTEX_ON();

    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC2_BADDR_A, (unsigned long)(addr) & 0xfffe0000);
    aml_sdio_bottom_read(SDIO_FUNC2, (SYS_TYPE)addr & SDIO_ADDR_MASK,
        (unsigned char*)&regdata, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    AML_WIFI_IPC_MUTEX_OFF();

    return regdata;
}

void aml_sdio_random_ram_write(unsigned char *buf, unsigned char *addr, size_t len)
{
    AML_WIFI_IPC_MUTEX_ON();

    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC2_BADDR_A, (unsigned long)(addr) & 0xfffe0000);
    aml_sdio_bottom_write(SDIO_FUNC2, ((SYS_TYPE)addr & SDIO_ADDR_MASK),
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    AML_WIFI_IPC_MUTEX_OFF();
}

void aml_sdio_random_ram_read(unsigned char* buf, unsigned char* addr, size_t len)
{
    AML_WIFI_IPC_MUTEX_ON();

    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC2_BADDR_A, (unsigned long)(addr) & 0xfffe0000);
    aml_sdio_bottom_read(SDIO_FUNC2, (SYS_TYPE)addr & SDIO_ADDR_MASK,
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    AML_WIFI_IPC_MUTEX_OFF();
}

//func3 for sram
void aml_sdio_sram_word_write(unsigned int addr, unsigned int data)
{
    unsigned int len = sizeof(unsigned int);
    aml_sdio_bottom_write(SDIO_FUNC3, (SYS_TYPE)addr % MAC_SRAM_BASE,
        (unsigned char *)&data, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));
}

unsigned int aml_sdio_sram_word_read(unsigned int addr)
{
    unsigned int regdata = 0;
    unsigned int len = sizeof(unsigned int);

    aml_sdio_bottom_read(SDIO_FUNC3, (SYS_TYPE)addr % MAC_SRAM_BASE,
        (unsigned char*)&regdata, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    return regdata;
}

void aml_sdio_sram_write(unsigned char *buf, unsigned char *addr, size_t len)
{
    aml_sdio_bottom_write(SDIO_FUNC3, (SYS_TYPE)addr % MAC_SRAM_BASE,
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));
}

void aml_sdio_sram_read(unsigned char* buf, unsigned char* addr, size_t len)
{
    aml_sdio_bottom_read(SDIO_FUNC3, (SYS_TYPE)addr % MAC_SRAM_BASE,
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));
}

//sdio func4 for tx buffer write/read
void aml_sdio_func4_set_base_addr(unsigned int addr, size_t len)
{
    if ((addr % tx_buffer_base_addr) >= SRAM_MAX_LEN || addr < tx_buffer_base_addr
        || ((addr % tx_buffer_base_addr) < SRAM_MAX_LEN && ((addr + len) % tx_buffer_base_addr) >= SRAM_MAX_LEN)) {
        tx_buffer_base_addr = addr;
        aml_sdio_self_define_domain_write32(RG_SCFG_FUNC4_BADDR_A, tx_buffer_base_addr);
    }
}

void aml_sdio_tx_buffer_write(unsigned char *buf, unsigned char *addr, size_t len)
{
    AML_WIFI_IPC_MUTEX_ON();

    aml_sdio_func4_set_base_addr((unsigned long)addr, len);
    aml_sdio_bottom_write(SDIO_FUNC4, ((SYS_TYPE)addr % tx_buffer_base_addr),
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    AML_WIFI_IPC_MUTEX_OFF();
}

void aml_sdio_tx_buffer_read(unsigned char* buf, unsigned char* addr, size_t len)
{
    AML_WIFI_IPC_MUTEX_ON();

    aml_sdio_func4_set_base_addr((unsigned long)addr, len);
    aml_sdio_bottom_read(SDIO_FUNC4, ((SYS_TYPE)addr % tx_buffer_base_addr),
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));

    AML_WIFI_IPC_MUTEX_OFF();
}


//sdio func5 for rx desc
void aml_sdio_desc_read(unsigned char* buf, unsigned char* addr, size_t len)
{
    aml_sdio_bottom_read(SDIO_FUNC5, ((SYS_TYPE)addr % RG_WIFI_IF_FW2HST_IRQ_CFG),
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));
}

void aml_sdio_func6_set_base_addr(unsigned int addr)
{
    if (addr >= (RXBUF_START_ADDR + LEN_128K)) {
        if (rx_buffer_base_addr != (RXBUF_START_ADDR + LEN_128K)) {
            rx_buffer_base_addr = RXBUF_START_ADDR + LEN_128K;
            aml_sdio_self_define_domain_write32(RG_SCFG_FUNC6_BADDR_A, rx_buffer_base_addr);
        }
    } else {
        if (rx_buffer_base_addr != RXBUF_START_ADDR) {
            rx_buffer_base_addr = RXBUF_START_ADDR;
            aml_sdio_self_define_domain_write32(RG_SCFG_FUNC6_BADDR_A, rx_buffer_base_addr);
        }
    }
}

//func6 for rx buffer
void aml_sdio_func6_rx_buffer_read(unsigned char *buf, unsigned char *addr, size_t len, unsigned char scat_use)
{
    unsigned char func_type = SDIO_FUNC6;
    if (scat_use)
        func_type |= 1<<8;
    aml_sdio_func6_set_base_addr((unsigned long)addr);
    aml_sdio_bottom_read(func_type, ((SYS_TYPE)addr % rx_buffer_base_addr),
        buf, len, SDIO_OPMODE_INCREMENT);
}
void aml_sdio_rx_buffer_read(unsigned char *buf, unsigned char *addr, size_t len, unsigned char scat_use)
{
    if (len > LEN_128K) {
        aml_sdio_func6_rx_buffer_read(buf, addr, LEN_128K, scat_use);
        aml_sdio_func6_rx_buffer_read(buf + LEN_128K, addr + LEN_128K, len - LEN_128K, scat_use);
    } else {
        aml_sdio_func6_rx_buffer_read(buf, addr, len, scat_use);
    }
}

//sdio func7 for bt
void aml_bt_sdio_read_sram(unsigned char *buf, unsigned char *addr, SYS_TYPE len)
{
    aml_sdio_bottom_read(SDIO_FUNC7, ((SYS_TYPE)addr & SDIO_ADDR_MASK),
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));
}

void aml_bt_sdio_write_sram(unsigned char *buf, unsigned char *addr, SYS_TYPE len)
{
    aml_sdio_bottom_write(SDIO_FUNC7, ((SYS_TYPE)addr & SDIO_ADDR_MASK),
        buf, len, (len > 8 ? SDIO_OPMODE_INCREMENT : SDIO_OPMODE_FIXED));
}

unsigned int aml_bt_hi_read_word(unsigned int addr)
{
    unsigned int regdata = 0;
    unsigned int reg_tmp;
    /*
     * make sure function 5 section address-mapping feature is disabled,
     * when this feature is disabled,
     * all 128k space in one sdio-function use only
     * one address-mapping: 32-bit AHB Address = BaseAddr + cmdRegAddr
     */

    reg_tmp = aml_sdio_self_define_domain_read32( RG_SDIO_IF_MISC_CTRL);

    if (!(reg_tmp & BIT(25))) {
        reg_tmp |= BIT(25);
        aml_sdio_self_define_domain_write32( RG_SDIO_IF_MISC_CTRL, reg_tmp);
    }

    /*config msb 15 bit address in BaseAddr Register*/
    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC7_BADDR_A,addr & 0xfffe0000);
    aml_bt_sdio_read_sram((unsigned char*)(SYS_TYPE)&regdata,
        /*sdio cmd 52/53 can only take 17 bit address*/
        (unsigned char*)(SYS_TYPE)(addr & 0x1ffff), sizeof(unsigned int));

    return regdata;
}

void aml_bt_hi_write_word(unsigned int addr,unsigned int data)
{
    unsigned int reg_tmp;
    /*
     * make sure function 5 section address-mapping feature is disabled,
     * when this feature is disabled,
     * all 128k space in one sdio-function use only
     * one address-mapping: 32-bit AHB Address = BaseAddr + cmdRegAddr
     */
    reg_tmp = aml_sdio_self_define_domain_read32( RG_SDIO_IF_MISC_CTRL);

    if (!(reg_tmp & BIT(25))) {
        reg_tmp |= BIT(25);
        aml_sdio_self_define_domain_write32( RG_SDIO_IF_MISC_CTRL, reg_tmp);
    }
    /*config msb 15 bit address in BaseAddr Register*/
    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC7_BADDR_A, addr & 0xfffe0000);
    aml_bt_sdio_write_sram((unsigned char *)&data,
        /*sdio cmd 52/53 can only take 17 bit address*/
        (unsigned char*)(SYS_TYPE)(addr & 0x1ffff), sizeof(unsigned int));
}

unsigned int aml_sdio_read_word(unsigned int addr)
{
    unsigned int regdata = 0;

    // for bt access always on reg
    if (((addr & 0x00f00000) == 0x00f00000) || ((addr & 0x00f00000) == 0x00b00000)
        || ((addr & 0x00f00000) == 0x00d00000) || ((addr & 0x00f00000) == 0x00900000)
        || ((addr & 0x00f00000) == 0x00200000) || ((addr & 0x00f00000) == 0x00300000)
        || ((addr & 0x00f00000) == 0x00400000)) {
        regdata = aml_bt_hi_read_word(addr);
    }

    return regdata;
}

void aml_sdio_write_word(unsigned int addr, unsigned int data)
{
    // for bt access always on reg
    if (((addr & 0x00f00000) == 0x00f00000) || ((addr & 0x00f00000) == 0x00b00000)
        || ((addr & 0x00f00000) == 0x00d00000) || ((addr & 0x00f00000) == 0x00900000)
        || ((addr & 0x00f00000) == 0x00200000) || ((addr & 0x00f00000) == 0x00300000)
        || ((addr & 0x00f00000) == 0x00400000)) {
        aml_bt_hi_write_word(addr, data);
    }
}

int aml_sdio_suspend(unsigned int suspend_enable)
{
    mmc_pm_flag_t flags;
    struct sdio_func *func = NULL;
    int ret = 0, i;

    if (suspend_enable == 0)
    {
        /* when enable == 0 that's resume operation
        and we do nothing for resume now. */
        return ret;
    }

    /* just clear sdio clock value for emmc init when resume */
    //amlwifi_set_sdio_host_clk(0);

    AML_BT_WIFI_MUTEX_ON();
    /* we shall suspend all card for sdio. */
    for (i = SDIO_FUNC1; i <= FUNCNUM_SDIO_LAST; i++)
    {
        func = aml_priv_to_func(i);
        if (func == NULL)
            continue;
        flags = sdio_get_host_pm_caps(func);

        if ((flags & MMC_PM_KEEP_POWER) != 0)
            ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);

        if (ret != 0) {
            AML_BT_WIFI_MUTEX_OFF();
            return -1;
        }

        /*
         * if we don't use sdio irq, we can't get functions' capability with
         * MMC_PM_WAKE_SDIO_IRQ, so we don't need set irq for wake up
         * sdio for upcoming suspend.
         */
        if ((flags & MMC_PM_WAKE_SDIO_IRQ) != 0)
            ret = sdio_set_host_pm_flags(func, MMC_PM_WAKE_SDIO_IRQ);

        if (ret != 0) {
            AML_BT_WIFI_MUTEX_OFF();
            return -1;
        }
    }

    AML_BT_WIFI_MUTEX_OFF();
    return ret;
}

void aml_sdio_scat_complete (struct amlw_hif_scatter_req * scat_req)
{
    struct aml_hwif_sdio *hif_sdio = &g_hwif_sdio;

    ASSERT(scat_req != NULL);
    ASSERT(hif_sdio != NULL);

    if (!scat_req) {
        printk("scar_req is NULL!\n");
        return;
    }

    scat_req->free = true;
    scat_req->scat_count = 0;
    scat_req->len = 0;
    scat_req->addr = 0;
    memset(scat_req->sgentries, 0, MAX_SG_ENTRIES * sizeof(struct scatterlist));
}

int aml_sdio_scat_req_rw(struct amlw_hif_scatter_req *scat_req)
{
    struct aml_hwif_sdio *hif_sdio = &g_hwif_sdio;
    struct sdio_func *func = NULL;
    struct mmc_host *host = NULL;

    unsigned int blk_size, blk_num;
    unsigned int max_blk_count, max_req_size;
    unsigned int func_num;

    struct scatterlist *sg;
    int sg_count, sgitem_count;
    int ttl_len, pkt_offset, ttl_page_num;

    struct mmc_request mmc_req;
    struct mmc_command mmc_cmd;
    struct mmc_data mmc_dat;
    //unsigned int reg_data = 0;

    int result = SDIOH_API_RC_FAIL;

    ASSERT(scat_req != NULL);
    if (scat_req->req & HIF_WRITE)
        func_num = SDIO_FUNC4;
    else
        func_num = SDIO_FUNC6;

    func = hif_sdio->sdio_func_if[func_num];
    host = func->card->host;

    blk_size = func->cur_blksize;
    max_blk_count = MIN(host->max_blk_count, SDIO_MAX_BLK_CNT); //host->max_blk_count: 511
    max_req_size = MIN(max_blk_count * blk_size, host->max_req_size); //host->max_req_size: 0x20000

    /* fill SG entries */
    sg = scat_req->sgentries;
    pkt_offset = 0;	    // reminder
    sgitem_count = 0; // count of scatterlist

    while (sgitem_count < scat_req->scat_count)
    {
        ttl_len = 0;
        sg_count = 0;
        ttl_page_num = 0;

        sg_init_table(sg, MAXSG_SIZE);

        /* assemble SG list */
        while ((sgitem_count < scat_req->scat_count) && (ttl_len < max_req_size))
        {
            int packet_len = 0;
            int sg_data_size = 0;
            unsigned char *pdata = NULL;

            if (sg_count >= MAXSG_SIZE)
                break;

            packet_len = scat_req->scat_list[sgitem_count].len;
            pdata = scat_req->scat_list[sgitem_count].packet;

            // sg len must be aligned with block size
            sg_data_size = ALIGN(packet_len, blk_size);
            if (sg_data_size > (max_req_size - ttl_len))
            {
                printk(" setup scat-data: (%s): %d: sg_data_size %d, remain %d \n",
                    __func__, __LINE__, sg_data_size, max_req_size - ttl_len);
                break;
            }

            sg_set_buf(&scat_req->sgentries[sg_count], pdata, sg_data_size);
            sg_count++;
            ttl_len += sg_data_size;

            ttl_page_num += scat_req->scat_list[sgitem_count].page_num;
            sgitem_count++;

            //printk("setup scat-data: offset: %d: ttl: %d, datalen:%d\n",
            //pkt_offset, ttl_len, sg_data_size);

        }

        if ((ttl_len == 0) || (ttl_len % blk_size != 0))
        {
            printk(" setup scat-data: (%s): %d: ttl_len %d \n",
                __func__, __LINE__, ttl_len);
            return result;
        }

        memset(&mmc_req, 0, sizeof(struct mmc_request));
        memset(&mmc_cmd, 0, sizeof(struct mmc_command));
        memset(&mmc_dat, 0, sizeof(struct mmc_data));

        /* set scatter-gather table for request */
        blk_num = ttl_len / blk_size;
        mmc_dat.flags = (scat_req->req & HIF_WRITE) ? MMC_DATA_WRITE : MMC_DATA_READ;
        mmc_dat.sg = scat_req->sgentries;
        mmc_dat.sg_len = sg_count;
        mmc_dat.blksz = blk_size;
        mmc_dat.blocks = blk_num;

        mmc_cmd.opcode = SD_IO_RW_EXTENDED;
        mmc_cmd.arg = (scat_req->req & HIF_WRITE) ? 1 << 31 : 0;
        mmc_cmd.arg |= (func_num & 0x7) << 28;
        /* block basic */
        mmc_cmd.arg |= 1 << 27;
        /* 0, fix address */
        mmc_cmd.arg |= SDIO_OPMODE_FIXED << 26;
        mmc_cmd.arg |= (scat_req->addr & 0x1ffff)<< 9;
        mmc_cmd.arg |= mmc_dat.blocks & 0x1ff;
        mmc_cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;

        mmc_req.cmd = &mmc_cmd;
        mmc_req.data = &mmc_dat;

        sdio_claim_host(func);
        mmc_set_data_timeout(&mmc_dat, func->card);
        mmc_wait_for_req(func->card->host, &mmc_req);
        sdio_release_host(func);

       // printk("setup scat-data: (%s) ====addr: 0x%X, (blksz: %d, blocks: %d) , (ttl:%d,sg:%d,scat_count:%d,ttl_page:%d)====\n",
           // (scat_req->req & HIF_WRITE) ? "wr" : "rd", scat_req->addr,
           // mmc_dat.blksz, mmc_dat.blocks, ttl_len,
           // sg_count, scat_req->scat_count, ttl_page_num);

        if (mmc_cmd.error || mmc_dat.error)
        {
            ERROR_DEBUG_OUT("ERROR CMD53 %s cmd_error = %d data_error=%d\n",
                (scat_req->req & HIF_WRITE) ? "write" : "read", mmc_cmd.error, mmc_dat.error);
        }

    }

    result = mmc_cmd.error ? mmc_cmd.error : mmc_dat.error;

    scat_req->result = result;

    if (scat_req->result)
        ERROR_DEBUG_OUT("Scatter write request failed:%d\n", scat_req->result);

    if (scat_req->req & HIF_ASYNCHRONOUS)
        aml_sdio_scat_complete(scat_req);

    return result;
}


EXPORT_SYMBOL(g_mmc_misc);
int aml_sdio_scat_req_rx_read(struct amlw_hif_scatter_req *scat_req)
{
    uint32_t func_num = SDIO_FUNC6;
    struct sdio_func *func = aml_priv_to_func(func_num);
    struct mmc_host *host = func->card->host;
    struct mmc_misc *mmc_misc = (struct mmc_misc *)g_mmc_misc;

    uint8_t *pdata = NULL;
    int result = SDIOH_API_RC_SUCCESS;
    uint32_t blk_size = func->cur_blksize;
    uint32_t max_blk_count = MIN(host->max_blk_count, SDIO_MAX_BLK_CNT);
    uint32_t max_req_size = MIN(max_blk_count * blk_size, host->max_req_size);
    uint32_t i = 0, sg_count = 0, packet_len = 0, packet_addr = 0;

    ASSERT_ERR(scat_req);

    memset(mmc_misc, 0, sizeof(struct mmc_misc) * scat_req->scat_count);

    sg_init_table(scat_req->sgentries, MAXSG_SIZE);

    while (sg_count < scat_req->scat_count) {

        if (sg_count >= MAXSG_SIZE) {
            printk("%s, %d: error sg_count: %d\n", __func__, __LINE__, sg_count);
            result = SDIOH_API_RC_FAIL;
            break;
        }

        pdata = scat_req->scat_list[sg_count].packet;
        packet_len = scat_req->scat_list[sg_count].len;
        packet_addr = scat_req->scat_list[sg_count].page_num;

        if ((packet_len == 0) || (packet_len % blk_size != 0) || (packet_len > max_req_size)) {
            printk("%s, %d: error packet_len: %d\n", __func__, __LINE__, packet_len);
            result = SDIOH_API_RC_FAIL;
            break;
        }

        sg_set_buf(&scat_req->sgentries[sg_count], pdata, packet_len);

        mmc_misc[sg_count].mmc_dat.flags = MMC_DATA_READ;
        mmc_misc[sg_count].mmc_dat.sg = &scat_req->sgentries[sg_count];
        mmc_misc[sg_count].mmc_dat.sg_len = 1;
        mmc_misc[sg_count].mmc_dat.blksz = blk_size;
        mmc_misc[sg_count].mmc_dat.blocks = packet_len / blk_size;

        mmc_misc[sg_count].mmc_cmd.opcode = SD_IO_RW_EXTENDED;
        mmc_misc[sg_count].mmc_cmd.arg |= (func_num & 0x7) << 28;
        mmc_misc[sg_count].mmc_cmd.arg |= 1 << 27;
        mmc_misc[sg_count].mmc_cmd.arg |= SDIO_OPMODE_FIXED << 26;
        mmc_misc[sg_count].mmc_cmd.arg |= (packet_addr & 0x1FFFF) << 9;
        mmc_misc[sg_count].mmc_cmd.arg |= mmc_misc[sg_count].mmc_dat.blocks & 0x1FF;
        mmc_misc[sg_count].mmc_cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;

        mmc_misc[sg_count].mmc_req.cmd = &mmc_misc[sg_count].mmc_cmd;
        mmc_misc[sg_count].mmc_req.data = &mmc_misc[sg_count].mmc_dat;

        sg_count++;
    }

    sdio_claim_host(func);
    mmc_set_data_timeout(&mmc_misc[i].mmc_dat, func->card);
    for (i = 0; i < sg_count; i++) {
        mmc_wait_for_req(func->card->host, &mmc_misc[i].mmc_req);

        if (mmc_misc[i].mmc_cmd.error || mmc_misc[i].mmc_dat.error) {
            printk("%s, %d: mmc_cmd error: %d; mmc_data error: %d\n", __func__, __LINE__, mmc_misc[i].mmc_cmd.error, mmc_misc[i].mmc_dat.error);
            result = mmc_misc[i].mmc_cmd.error ? mmc_misc[i].mmc_cmd.error : mmc_misc[i].mmc_dat.error;
            break;
        }
    }
    sdio_release_host(func);

    scat_req->free = true;
    scat_req->scat_count = 0;
    scat_req->len = 0;
    scat_req->addr = 0;
    memset(scat_req->sgentries, 0, MAX_SG_ENTRIES * sizeof(struct scatterlist));

    return result;
}

struct amlw_hif_scatter_req *aml_sdio_scatter_req_get(struct aml_hwif_sdio *hif_sdio)
{
    struct amlw_hif_scatter_req *scat_req = NULL;

    ASSERT(hif_sdio != NULL);

    scat_req = hif_sdio->scat_req;

    if (scat_req->free)
    {
        scat_req->free = false;
    }
    else if (scat_req->scat_count != 0) // get scat_req, but not build scatter list
    {
        scat_req = NULL;
    }

    return scat_req;
}

static int amlw_sdio_alloc_prep_scat_req(struct aml_hwif_sdio *hif_sdio)
{
    struct amlw_hif_scatter_req * scat_req = NULL;

    if (!hif_sdio) {
        printk("hif_sdio is NULL!\n");
        return 1;
    }

    /* allocate the scatter request */
    scat_req = ZMALLOC(sizeof(struct amlw_hif_scatter_req), "sdio_write", GFP_KERNEL);
    if (scat_req == NULL)
    {
        ERROR_DEBUG_OUT("[sdio sg alloc_scat_req]: no mem\n");
        return 1;
    }

    scat_req->free = true;
    hif_sdio->scat_req = scat_req;

    return 0;
}

int aml_sdio_enable_scatter(struct aml_hwif_sdio *hif_sdio)
{
    int ret;

    ASSERT(hif_sdio != NULL);

    if (hif_sdio->scatter_enabled)
        return 0;

    // TODO : getting hw_config to configure scatter number;

    hif_sdio->scatter_enabled = true;

    ret = amlw_sdio_alloc_prep_scat_req(hif_sdio);
    return ret;
}

int aml_sdio_scat_rw(struct scatterlist *sg_list, unsigned int sg_num, unsigned int blkcnt,
        unsigned char func_num, unsigned int addr, unsigned char write)
{
    struct mmc_request mmc_req;
    struct mmc_command mmc_cmd;
    struct mmc_data    mmc_dat;
    struct sdio_func *func = aml_priv_to_func(func_num);
    int ret = 0;

    AML_BT_WIFI_MUTEX_ON();
    memset(&mmc_req, 0, sizeof(struct mmc_request));
    memset(&mmc_cmd, 0, sizeof(struct mmc_command));
    memset(&mmc_dat, 0, sizeof(struct mmc_data));

    mmc_dat.sg     = sg_list;
    mmc_dat.sg_len = sg_num;
    mmc_dat.blksz  = FUNC4_BLKSIZE;
    mmc_dat.blocks = blkcnt;
    mmc_dat.flags  = write ? MMC_DATA_WRITE : MMC_DATA_READ;

    mmc_cmd.opcode = SD_IO_RW_EXTENDED;
    mmc_cmd.arg    = write ? 1 << 31 : 0;
    mmc_cmd.arg   |= (func_num & 0x7) << 28;
    mmc_cmd.arg   |= 1 << 27;	/* block basic */
    mmc_cmd.arg   |= 0 << 26;	/* 1 << 26;*/   	/*0 fix address */
    mmc_cmd.arg   |= (addr & 0x1ffff)<< 9;
    mmc_cmd.arg   |= blkcnt & 0x1ff;
    mmc_cmd.flags  = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;

    mmc_req.cmd = &mmc_cmd;
    mmc_req.data = &mmc_dat;

    sdio_claim_host(func);
    mmc_set_data_timeout(&mmc_dat, func->card);
    mmc_wait_for_req(func->card->host, &mmc_req);
    sdio_release_host(func);

    if (mmc_cmd.error || mmc_dat.error) {
        printk("ERROR CMD53 %s cmd_error = %d data_error=%d\n",
               write ? "write" : "read", mmc_cmd.error, mmc_dat.error);
        ret  = mmc_cmd.error;
    }

    AML_BT_WIFI_MUTEX_OFF();
    return ret;
}

void aml_sdio_cleanup_scatter(struct aml_hwif_sdio *hif_sdio)
{
    printk("[sdio sg cleanup]: enter\n");

    ASSERT(hif_sdio != NULL);

    if (!hif_sdio->scatter_enabled)
        return;

    hif_sdio->scatter_enabled = false;

    /* empty the free list */
    FREE(hif_sdio->scat_req, "sdio_write");
    printk("[sdio sg cleanup]: exit\n");

    return;
}

void aml_sdio_init_ops(void)
{
    struct aml_hif_sdio_ops* ops = &g_hif_sdio_ops;

    //func1 operation func, read/write self define domain reg, no need to set base addr
    ops->hi_self_define_domain_write8 = aml_sdio_self_define_domain_write8;
    ops->hi_self_define_domain_read8 = aml_sdio_self_define_domain_read8;
    ops->hi_self_define_domain_write32 = aml_sdio_self_define_domain_write32;
    ops->hi_self_define_domain_read32 = aml_sdio_self_define_domain_read32;

    //func2 operation func, need to set base addr firstly
    ops->hi_random_word_write = aml_sdio_random_word_write;
    ops->hi_random_word_read = aml_sdio_random_word_read;
    ops->hi_random_ram_write = aml_sdio_random_ram_write;
    ops->hi_random_ram_read = aml_sdio_random_ram_read;

    //func3 sram operation func
    ops->hi_sram_word_write = aml_sdio_sram_word_write;
    ops->hi_sram_word_read = aml_sdio_sram_word_read;
    ops->hi_sram_write = aml_sdio_sram_write;
    ops->hi_sram_read = aml_sdio_sram_read;

    //func4 tx buffer
    ops->hi_tx_buffer_write = aml_sdio_tx_buffer_write;
    ops->hi_tx_buffer_read = aml_sdio_tx_buffer_read;

    //func5 rx desc
    ops->hi_desc_read = aml_sdio_desc_read;

    //func6, rx buffer read func
    ops->hi_rx_buffer_read = aml_sdio_rx_buffer_read;

    //for scatter list
    ops->hi_enable_scat = aml_sdio_enable_scatter;
    ops->hi_cleanup_scat = aml_sdio_cleanup_scatter;
    ops->hi_get_scatreq = aml_sdio_scatter_req_get;
    ops->hi_send_frame = aml_sdio_scat_req_rw;
    ops->hi_recv_frame = aml_sdio_scat_req_rx_read;

    //sdio func7 for bt
    ops->bt_hi_write_sram = aml_bt_sdio_write_sram;
    ops->bt_hi_read_sram = aml_bt_sdio_read_sram;
    ops->bt_hi_write_word = aml_bt_hi_write_word;
    ops->bt_hi_read_word = aml_bt_hi_read_word;

    //for suspend & resume
    ops->hif_suspend = aml_sdio_suspend;

    g_sdio_after_porbe = 1;

    // check and wake firstly.
    host_wake_req = NULL;
    host_suspend_req = NULL;
}

void aml_sdio_init_base_addr(void)
{
    g_func_kmalloc_buf = (unsigned char *)ZMALLOC(LEN_128K, "func_sdio_read", GFP_DMA | GFP_ATOMIC);
    if (!g_func_kmalloc_buf) {
         printk(">>>sdio kmalloc failed!");
    }

    //func3, config sram base addr
    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC3_BADDR_A, MAC_SRAM_BASE);

    //func4, config tx buffer base addr
    tx_buffer_base_addr = (TXBUF_START_ADDR);
    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC4_BADDR_A, tx_buffer_base_addr);

    //func5, rxdesc base addr
    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC5_BADDR_A, RG_WIFI_IF_FW2HST_IRQ_CFG);

    //func6, rx buffer base addr
    rx_buffer_base_addr = (RXBUF_START_ADDR);
    aml_sdio_self_define_domain_write32(RG_SCFG_FUNC6_BADDR_A, rx_buffer_base_addr);
}

int aml_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
    int ret = 0;
    static struct sdio_func sdio_func_0;

    sdio_claim_host(func);
    ret = sdio_enable_func(func);
    if (ret)
        goto sdio_enable_error;

    if (func->num == 4)
        sdio_set_block_size(func, 512);
    else
        sdio_set_block_size(func, 512);

    printk("%s(%d): func->num %d sdio block size=%d, \n", __func__, __LINE__,
        func->num,  func->cur_blksize);

    if (func->num == 1)
    {
        sdio_func_0.num = 0;
        sdio_func_0.card = func->card;
        g_hwif_sdio.sdio_func_if[0] = &sdio_func_0;
    }
    g_hwif_sdio.sdio_func_if[func->num] = func;
    printk("%s(%d): func->num %d sdio_func=%p, \n", __func__, __LINE__,
        func->num,  func);

    sdio_release_host(func);
    sdio_set_drvdata(func, (void *)(&g_hwif_sdio));
    if (func->num != FUNCNUM_SDIO_LAST)
    {
        printk("%s(%d):func_num=%d, last func num=%d\n", __func__, __LINE__,
            func->num, FUNCNUM_SDIO_LAST);
        return 0;
    }

    aml_sdio_init_ops();
    aml_sdio_init_base_addr();
    g_hif_sdio_ops.hi_enable_scat(&g_hwif_sdio);
    g_hif_sdio_ops.hi_enable_scat(&g_hwif_rx_sdio);

    return ret;

sdio_enable_error:
    printk("sdio_enable_error:  line %d\n",__LINE__);
    sdio_release_host(func);

    return ret;
}

static void  aml_sdio_remove(struct sdio_func *func)
{
    if (func== NULL)
    {
        return ;
    }

    printk("\n==========================================\n");
    printk("aml_sdio_remove++ func->num =%d \n",func->num);
    printk("==========================================\n");

    sdio_claim_host(func);
    sdio_disable_func(func);
    sdio_release_host(func);

    host_wake_req = NULL;
    host_suspend_req = NULL;
    host_resume_req = NULL;
}

static int aml_sdio_pm_suspend(struct device *device)
{
    if (host_suspend_req != NULL)
        return host_suspend_req(device);
    else
        return aml_sdio_suspend(1);
}

static int aml_sdio_pm_resume(struct device *device)
{
    if (host_resume_req != NULL)
        return host_resume_req(device);
    else
        return 0;
}

static SIMPLE_DEV_PM_OPS(aml_sdio_pm_ops, aml_sdio_pm_suspend,
                     aml_sdio_pm_resume);


static const struct sdio_device_id aml_sdio[] =
{
    {SDIO_DEVICE(W2_VENDOR_AMLOGIC,W2_PRODUCT_AMLOGIC) },
    {SDIO_DEVICE(W2_VENDOR_AMLOGIC_EFUSE,W2_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W2s_VENDOR_AMLOGIC_EFUSE,W2s_PRODUCT_AMLOGIC_EFUSE)},
    {}
};

static struct sdio_driver aml_sdio_driver =
{
    .name = "aml_sdio",
    .id_table = aml_sdio,
    .probe = aml_sdio_probe,
    .remove = aml_sdio_remove,
    .drv.pm = &aml_sdio_pm_ops,
};

int  aml_sdio_init(void)
{
    int err = 0;

    //amlwifi_set_sdio_host_clk(200000000);//200MHZ

    err = sdio_register_driver(&aml_sdio_driver);
    g_sdio_driver_insmoded = 1;
    g_wifi_in_insmod = 0;
    printk("*****************aml sdio common driver is insmoded********************\n");
    if (err)
        printk("failed to register sdio driver: %d \n", err);

    return err;
}
EXPORT_SYMBOL(aml_sdio_init);

void  aml_sdio_exit(void)
{
    printk("aml_sdio_exit++ \n");
    sdio_unregister_driver(&aml_sdio_driver);
    g_sdio_driver_insmoded = 0;
    g_sdio_after_porbe = 0;
    if (g_func_kmalloc_buf) {
        FREE(g_func_kmalloc_buf, "func_sdio_read");
        g_func_kmalloc_buf = NULL;
    }
    printk("*****************aml sdio common driver is rmmoded********************\n");
}
//EXPORT_SYMBOL(aml_sdio_exit);

EXPORT_SYMBOL(g_sdio_driver_insmoded);
EXPORT_SYMBOL(g_wifi_in_insmod);
EXPORT_SYMBOL(g_sdio_after_porbe);
EXPORT_SYMBOL(host_wake_req);
EXPORT_SYMBOL(host_suspend_req);
EXPORT_SYMBOL(host_resume_req);
/*set_wifi_bt_sdio_driver_bit() is used to determine whether to unregister sdio power driver.
  *Only when g_sdio_wifi_bt_alive is 0, then call aml_sdio_exit().
*/
void set_wifi_bt_sdio_driver_bit(bool is_register, int shift)
{
    AML_BT_WIFI_MUTEX_ON();
    if (is_register) {
        g_sdio_wifi_bt_alive |= (1 << shift);
        printk("Insmod %s sdio driver!\n", (shift ? "WiFi":"BT"));
    } else {
        printk("Rmmod %s sdio driver!\n", (shift ? "WiFi":"BT"));
        g_sdio_wifi_bt_alive &= ~(1 << shift);
        if (!g_sdio_wifi_bt_alive) {
            aml_sdio_exit();
        }
    }
    AML_BT_WIFI_MUTEX_OFF();
}
EXPORT_SYMBOL(set_wifi_bt_sdio_driver_bit);
EXPORT_SYMBOL(g_hwif_sdio);
EXPORT_SYMBOL(g_hwif_rx_sdio);
EXPORT_SYMBOL(g_hif_sdio_ops);

int aml_sdio_insmod(void)
{
    aml_sdio_init();
    printk("%s(%d) start...\n",__func__, __LINE__);
    return 0;
}

void aml_sdio_rmmod(void)
{
    aml_sdio_exit();
}

void aml_sdio_calibration(void)
{
    struct aml_hif_sdio_ops* hif_ops = &g_hif_sdio_ops;
    int err;
    unsigned char i, j, k, l;
    unsigned char step;

    step = 4;
    hif_ops->hi_self_define_domain_write8(0x2c0, 0);
    for (i = 0; i < 32; i += step) {
        hif_ops->hi_self_define_domain_write8(0x2c2, i);

        for (j = 0; j < 32; j += step) {
            hif_ops->hi_self_define_domain_write8(0x2c3, j);

            for (k = 0; k < 32; k += step) {
                hif_ops->hi_self_define_domain_write8(0x2c4, k);

                for (l = 0; l < 32; l += step) {
                    hif_ops->hi_self_define_domain_write8(0x2c5, l);

                    //msleep(3000);
                    err = hif_ops->hi_self_define_domain_write32(RG_SCFG_FUNC2_BADDR_A, l);

                    if (err) {
                        //msleep(3000);
                        hif_ops->hi_self_define_domain_write8(SDIO_CCCR_IOABORT, 0x1);
                        printk("%s error: i:%d, j:%d, k:%d, l:%d\n", __func__, i, j, k, l);

                    } else {
                        printk("%s right, use this config: i:%d, j:%d, k:%d, l:%d\n", __func__, i, j, k, l);
                        return;
                    }
                }
            }
        }
    }

    hif_ops->hi_self_define_domain_write8(0x2c2, 0);
    hif_ops->hi_self_define_domain_write8(0x2c3, 0);
    hif_ops->hi_self_define_domain_write8(0x2c4, 0);
    hif_ops->hi_self_define_domain_write8(0x2c5, 0);
}

void wifi_cpu_clk_switch(unsigned int clk_cfg)
{
    struct aml_hif_sdio_ops *hif_ops = &g_hif_sdio_ops;
    hif_ops->hi_random_word_write(RG_INTF_CPU_CLK, clk_cfg);

    printk("%s(%d):cpu_clk_reg=0x%08x\n", __func__, __LINE__,
    hif_ops->hi_random_word_read(RG_INTF_CPU_CLK));
}

#ifdef ICCM_CHECK
unsigned char buf_iccm_rd[ICCM_BUFFER_RD_LEN];
#endif

unsigned char aml_download_wifi_fw_img(char *firmware_filename)
{
    unsigned int offset_base = 0;
    size_t databyte = 0;
    unsigned int regdata =0;
    int i = 0, err = 0;
    unsigned int offset = 0;
    //unsigned int rom_len = 0;
    unsigned int tmp_val = 0;
    unsigned int len = 0;
    char tmp_buf[9] = {0};
    unsigned char *src = NULL;
    unsigned char *kmalloc_buf = NULL;
    const struct firmware *fw = NULL;
    struct aml_hif_sdio_ops *hif_ops = &g_hif_sdio_ops;
    unsigned int to_sdio = ~(0);
    RG_PMU_A22_FIELD_T pmu_a22;
    RG_DPLL_A5_FIELD_T rg_dpll_a5;
    struct sdio_func *func = aml_priv_to_func(SDIO_FUNC0);

    printk("%s: %d\n", __func__, __LINE__);
    err =request_firmware(&fw, firmware_filename, &func->dev);
    if (err) {
        ERROR_DEBUG_OUT("request firmware fail!\n");
        return err;
    }

#ifdef ICCM_ROM
    offset = ICCM_ROM_LEN;
    len = ICCM_RAM_LEN;
#endif

    src = (unsigned char *)fw->data + (offset / 4) * BYTE_IN_LINE;
    kmalloc_buf = (unsigned char *)ZMALLOC(len, "sdio_write", GFP_DMA | GFP_ATOMIC);
    if (!kmalloc_buf) {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        release_firmware(fw);
        return -ENOMEM;
    }

    for (i = 0; i < len /4; i++) {
        tmp_buf[8] = 0;
        strncpy(tmp_buf, (char *)src, 8);
        if ((err = kstrtouint(tmp_buf, 16, &tmp_val))) {
            release_firmware(fw);
            FREE(kmalloc_buf, "sdio_write");
            return err;
        }
        *(unsigned int *)&kmalloc_buf[4 * i] = __swab32(tmp_val);
        src += BYTE_IN_LINE;
    }

    // close phy rest
    hif_ops->hi_random_word_write(RG_WIFI_RST_CTRL, to_sdio);

#ifdef EFUSE_ENABLE
    efuse_init();
    printk("%s(%d): called efuse init\n", __func__, __LINE__);
#endif

    rg_dpll_a5.data = hif_ops->bt_hi_read_word(RG_DPLL_A5);
    printk("%s(%d): img len 0x%x, start download fw\n", __func__, __LINE__, len);

    do {
        databyte = (len > SRAM_MAX_LEN) ? SRAM_MAX_LEN : len;

        hif_ops->hi_random_ram_write(kmalloc_buf + offset_base,
            (unsigned char *)(SYS_TYPE)(MAC_ICCM_AHB_BASE + offset_base + ICCM_ROM_LEN), databyte);

        offset_base += databyte;
        len -= databyte;
    } while(len > 0);

#ifdef ICCM_CHECK
    offset_base =0;
    len = ICCM_CHECK_LEN;

    //host iccm ram read
    do {
        databyte = (len > SRAM_MAX_LEN) ? SRAM_MAX_LEN : len;

        hif_ops->hi_random_ram_read(buf_iccm_rd + offset_base,
            (unsigned char*)(SYS_TYPE)(MAC_ICCM_AHB_BASE + offset_base + ICCM_ROM_LEN), databyte);

        offset_base += databyte;
        len -= databyte;
    } while(len > 0);

    if (memcmp(buf_iccm_rd, kmalloc_buf, ICCM_CHECK_LEN)) {
        ERROR_DEBUG_OUT("Host HAL: write ICCM ERROR!!!! \n");
        release_firmware(fw);
        FREE(kmalloc_buf, "sdio_write");
        return false;

    } else {
        printk("Host HAL: write ICCM SUCCESS!!!! \n");
    }
#endif

    /* Starting download DCCM */
    src = (unsigned char *)fw->data + (ICCM_ALL_LEN / 4) * BYTE_IN_LINE;
    len = DCCM_ALL_LEN;
    offset_base = 0;

    for (i = 0; i < len /4; i++) {
        tmp_buf[8] = 0;
        strncpy(tmp_buf, (char *)src, 8);
        if ((err = kstrtouint(tmp_buf, 16, &tmp_val))) {
            release_firmware(fw);
            FREE(kmalloc_buf, "sdio_write");
            return err;
        }
        *(unsigned int *)&kmalloc_buf[4 * i] = __swab32(tmp_val);
        src += BYTE_IN_LINE;
    }

    printk("%s(%d): dccm img len 0x%x, start download dccm\n", __func__, __LINE__, len);
    do {
        databyte = (len > SRAM_MAX_LEN) ? SRAM_MAX_LEN : len;

        hif_ops->hi_random_ram_write(kmalloc_buf + offset_base,
            (unsigned char*)(SYS_TYPE)(MAC_DCCM_AHB_BASE + offset_base), databyte);

        offset_base += databyte;
        len -= databyte;
    } while(len > 0);


#if 1
    len = DCCM_CHECK_LEN;
    offset_base = 0;

    //host iccm ram read
    do {
        databyte = (len > SRAM_MAX_LEN) ? SRAM_MAX_LEN : len;

        hif_ops->hi_random_ram_read(buf_iccm_rd + offset_base,
            (unsigned char*)(SYS_TYPE)(MAC_DCCM_AHB_BASE + offset_base), databyte);

        offset_base += databyte;
        len -= databyte;
    } while(len > 0);

    if (memcmp(buf_iccm_rd, kmalloc_buf, DCCM_CHECK_LEN)) {
        ERROR_DEBUG_OUT("Host HAL: write DCCM ERROR!!!! \n");
        release_firmware(fw);
        FREE(kmalloc_buf, "sdio_write");
        return false;

    } else {
        printk("Host HAL: write DCCM SUCCESS!!!! \n");
    }
#endif

    release_firmware(fw);
    FREE(kmalloc_buf, "sdio_write");

    wifi_cpu_clk_switch(0x4f770033);
    /* mac clock 160 Mhz */
    hif_ops->hi_random_word_write(RG_INTF_MAC_CLK, 0x00030001);
    hif_ops->hi_random_word_write(RG_AON_A37, hif_ops->hi_random_word_read(RG_AON_A37) | 0x1);
    //cpu select riscv
    regdata = hif_ops->hi_random_word_read(RG_WIFI_CPU_CTRL);
    regdata |= 0x10000;
    hif_ops->hi_random_word_write(RG_WIFI_CPU_CTRL,regdata);
    printk("RG_WIFI_CPU_CTRL = %x redata= %x \n",RG_WIFI_CPU_CTRL,regdata);
    //enable cpu
    pmu_a22.data = hif_ops->bt_hi_read_word(RG_PMU_A22);
    pmu_a22.b.rg_dev_reset_sw = 0x00;
    hif_ops->bt_hi_write_word(RG_PMU_A22,pmu_a22.data);
    printk("RG_PMU_A22 = %x redata= %x \n",RG_PMU_A22,pmu_a22.data);
    printk("fw download success!\n");

    return true;
}

EXPORT_SYMBOL(aml_sdio_insmod);
EXPORT_SYMBOL(aml_sdio_rmmod);
EXPORT_SYMBOL(aml_sdio_calibration);
EXPORT_SYMBOL(aml_download_wifi_fw_img);
EXPORT_SYMBOL(aml_priv_to_func);