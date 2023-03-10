#include <linux/mutex.h>
#include "chip_ana_reg.h"
#include "chip_pmu_reg.h"
#include "chip_intf_reg.h"
#include "wifi_intf_addr.h"
#include "wifi_top_addr.h"
#include "wifi_sdio_cfg_addr.h"
#include "wifi_w2_shared_mem_cfg.h"
#include "sdio_common.h"
#include "sg_common.h"
#include "w1_sdio.h"
#include "aml_interface.h"
#include "w2_sdio.h"

struct aml_hwif_sdio g_hwif_sdio;
unsigned char g_sdio_wifi_bt_alive;
unsigned char g_sdio_driver_insmoded;
unsigned char g_sdio_after_porbe;
unsigned char g_wifi_in_insmod;
extern unsigned int chip_id;
unsigned char *g_func_kmalloc_buf = NULL;
unsigned char wifi_irq_enable = 0;
unsigned int  shutdown_i = 0;
unsigned char wifi_sdio_shutdown = 0;
extern unsigned char wifi_in_insmod;
extern unsigned char wifi_in_rmmod;
extern unsigned char  chip_en_access;
extern unsigned char wifi_sdio_shutdown;

static DEFINE_MUTEX(wifi_bt_sdio_mutex);
static DEFINE_MUTEX(wifi_ipc_mutex);

unsigned char (*host_wake_req)(void);
int (*host_suspend_req)(struct device *device);
int (*host_resume_req)(struct device *device);
extern void aml_w1_deinit_wlan_mem(void);
extern int aml_w1_init_wlan_mem(void);
extern chip_id_type aml_get_chip_type(void);

struct sdio_func *aml_priv_to_func(int func_n)
{
    ASSERT(func_n >= 0 &&  func_n < SDIO_FUNCNUM_MAX);
    return g_hwif_sdio.sdio_func_if[func_n];
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

int _aml_sdio_request_buffer(unsigned char func_num,
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


void aml_sdio_read_chip_type_reg(unsigned int addr)
{
    int result;
    int w_len = sizeof(unsigned long);
    int r_len = sizeof(unsigned int);
    void *kmalloc_buf;
    unsigned char *r_kmalloc_buf = NULL;
    unsigned int w_addr, r_addr;
    unsigned int regdata = 0;
    unsigned char*buf;
    unsigned long sram_data;

    AML_BT_WIFI_MUTEX_ON();

    kmalloc_buf =  (unsigned char *)ZMALLOC(w_len, "sdio_write", GFP_DMA);
    r_kmalloc_buf = (unsigned char *)ZMALLOC(r_len, "sdio_read", GFP_DMA);
    if (kmalloc_buf == NULL || r_kmalloc_buf == NULL) {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        AML_BT_WIFI_MUTEX_OFF();
        return ;
    }

    w_addr = RG_SCFG_FUNC2_BADDR_A & SDIO_ADDR_MASK;
    sram_data = ((unsigned long )(addr) & 0xfffe0000);
    memcpy(kmalloc_buf, &sram_data, w_len);
    result = _aml_sdio_request_buffer(SDIO_FUNC1, SDIO_OPMODE_INCREMENT, SDIO_WRITE, w_addr, kmalloc_buf, w_len);
    if (result < 0) {
        ERROR_DEBUG_OUT("_aml_sdio_request_buffer fail");
        return;
    }

    r_addr = (unsigned long )(addr) & SDIO_ADDR_MASK;
    buf = (unsigned char*)(&regdata);
    result = _aml_sdio_request_buffer(SDIO_FUNC2, SDIO_OPMODE_FIXED, SDIO_READ, r_addr, r_kmalloc_buf, r_len);
    if (result < 0) {
        ERROR_DEBUG_OUT("_aml_sdio_request_buffer fail");
        return;
    }

    if (r_kmalloc_buf != buf) {
        memcpy(buf, r_kmalloc_buf, r_len);
        FREE(r_kmalloc_buf, "sdio_read");
    }

    FREE(kmalloc_buf, "sdio_write");
    AML_BT_WIFI_MUTEX_OFF();

    chip_id |= regdata;
    printk("%s: ********get chipid:0x%08x \n", __func__, chip_id);
}

void aml_sdio_read_chip_type(void)
{
    aml_sdio_read_chip_type_reg(WIFI_CHIP1_TYPE_ADDR);
    aml_sdio_read_chip_type_reg(WIFI_CHIP2_TYPE_ADDR);
    printk("%s: ********get chipid:0x%08x \n", __func__, chip_id);
}

void aml_sdio_init_ops(void)
{
    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        printk("%s: init w2 ops\n", __func__);
        aml_sdio_init_w2_ops();
    } else if (aml_get_chip_type() == WIFI_CHIP_W1) {
        printk("%s: *********init w1 ops\n", __func__);
        aml_w1_sdio_init_ops();
    } else {
        printk("chong: %s err chipid: %d \n", __func__, aml_get_chip_type());
        return;
    }
    return;
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
    printk("%s: %d, sdio probe success\n", __func__, __LINE__);

    aml_sdio_read_chip_type();

    if (aml_get_chip_type() == WIFI_CHIP_W1) {
#ifdef NOT_AMLOGIC_PLATFORM
        aml_w1_init_wlan_mem();
#else
        aml_w1_deinit_wlan_mem();
#endif
    }
    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        aml_sdio_init_base_addr();
    }
    aml_sdio_init_ops();

    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        g_hif_sdio_ops.hi_enable_scat(&g_hwif_sdio);
        g_hif_sdio_ops.hi_enable_scat(&g_hwif_rx_sdio);
    }
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

 int aml_sdio_pm_suspend(struct device *device)
{
    if (host_suspend_req != NULL)
        return host_suspend_req(device);
    else
        return aml_sdio_suspend(1);
}

 int aml_sdio_pm_resume(struct device *device)
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
    {SDIO_DEVICE(W2s_VENDOR_AMLOGIC_EFUSE,W2s_A_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W2s_VENDOR_AMLOGIC_EFUSE,W2s_B_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W1_VENDOR_AMLOGIC,W1_PRODUCT_AMLOGIC) },
    {SDIO_DEVICE(W1_VENDOR_AMLOGIC_EFUSE,W1_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1us_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1us_A_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1us_B_PRODUCT_AMLOGIC_EFUSE)},
    {SDIO_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1us_C_PRODUCT_AMLOGIC_EFUSE)},
    {}
};

static struct sdio_driver aml_sdio_driver =
{
    .name = "aml_sdio",
    .id_table = aml_sdio,
    .probe = aml_sdio_probe,
    .remove = aml_sdio_remove,
    .drv.pm = &aml_sdio_pm_ops,
    .drv.shutdown = aml_sdio_shutdown,
};

int  aml_sdio_init(void)
{
    int err = 0;

    //amlwifi_set_sdio_host_clk(200000000);//200MHZ

    err = sdio_register_driver(&aml_sdio_driver);
    g_sdio_driver_insmoded = 1;
    g_wifi_in_insmod = 0;

    wifi_in_insmod = 0;
    wifi_in_rmmod = 0;
    chip_en_access = 0;
    wifi_sdio_shutdown = 0;
    printk("*****************aml sdio common driver is insmoded********************\n");
    if (err)
        printk("failed to register sdio driver: %d \n", err);

    return err;
}

void  aml_sdio_exit(void)
{
    printk("aml_sdio_exit++ \n");
    sdio_unregister_driver(&aml_sdio_driver);
    g_sdio_driver_insmoded = 0;
    g_sdio_after_porbe = 0;
    if (g_func_kmalloc_buf) {
        //FREE(g_func_kmalloc_buf, "func_sdio_read");
        g_func_kmalloc_buf = NULL;
    }
    printk("*****************aml sdio common driver is rmmoded********************\n");
}


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

extern int wifi_irq_num(void);
extern void config_pmu_reg_off(void);
void aml_sdio_shutdown(struct device *device)
{
    printk("===>>> enter %s <<<===\n", __func__);
    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        return;
    }
    if (wifi_irq_enable == 1) {

#ifdef USE_SDIO_IRQ
        struct sdio_func *func = g_hwif_sdio.sdio_func_if[SDIO_FUNC1];
        sdio_claim_host(func);
        sdio_release_irq(func);
        sdio_release_host(func);
#else
    #ifndef CONFIG_PT_MODE
        unsigned int irq_num = wifi_irq_num();
        disable_irq(irq_num);
    #endif
#endif

        wifi_irq_enable = 0;
    }
    shutdown_i += 1;
    if (shutdown_i == 1) {
        wifi_sdio_shutdown = 1;
        config_pmu_reg_off();
    } else if (shutdown_i == 7) {
        shutdown_i = 0;
        printk("===>>> end <<<===\n");
    } else {
        ;
    }
    printk("=== shutdown_i:%d ===\n", shutdown_i);
}


EXPORT_SYMBOL(wifi_irq_enable);
EXPORT_SYMBOL(aml_sdio_insmod);
EXPORT_SYMBOL(aml_sdio_rmmod);
EXPORT_SYMBOL(set_wifi_bt_sdio_driver_bit);
EXPORT_SYMBOL(g_hwif_sdio);
EXPORT_SYMBOL(aml_sdio_exit);
EXPORT_SYMBOL(aml_sdio_init);
EXPORT_SYMBOL(g_sdio_driver_insmoded);
EXPORT_SYMBOL(g_wifi_in_insmod);
EXPORT_SYMBOL(g_sdio_after_porbe);
EXPORT_SYMBOL(host_wake_req);
EXPORT_SYMBOL(host_suspend_req);
EXPORT_SYMBOL(host_resume_req);
EXPORT_SYMBOL(aml_priv_to_func);
