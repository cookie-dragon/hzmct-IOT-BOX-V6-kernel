/*
 * Code for AM335X EVM.
 *
 * Copyright (C) 2011 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/i2c/at24.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/wl12xx.h>
#include <linux/ethtool.h>
#include <linux/mfd/tps65910.h>
#include <linux/mfd/tps65217.h>
#include <linux/pwm_backlight.h>
#include <linux/input/ti_tsc.h>
#include <linux/platform_data/ti_adc.h>
#include <linux/mfd/ti_tscadc.h>
#include <linux/reboot.h>
#include <linux/pwm/pwm.h>
#include <linux/rtc/rtc-omap.h>
#include <linux/opp.h>

/* LCD controller is similar to DA850 */
#include <video/da8xx-fb.h>

#include <mach/hardware.h>
#include <mach/board-am335xevm.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/asp.h>

#include <plat/omap_device.h>
#include <plat/omap-pm.h>
#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/lcdc.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/emif.h>
#include <plat/nand.h>

#include "board-flash.h"
#include "cpuidle33xx.h"
#include "mux.h"
#include "devices.h"
#include "hsmmc.h"

/* Convert GPIO signal to GPIO pin number */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

#undef  SPI0
#define NANDBOOT

/* AM335X EVM Phy ID and Debug Registers */
#define AM335X_EVM_PHY_ID		0x7c0f1     //LAN8720
#define AM335X_EVM_PHY_MASK		0xfffffffe
#define AR8051_PHY_DEBUG_ADDR_REG	0x1d
#define AR8051_PHY_DEBUG_DATA_REG	0x1e
#define AR8051_DEBUG_RGMII_CLK_DLY_REG	0x5
#define AR8051_RGMII_TX_CLK_DLY		BIT(8)


static const struct display_panel disp_panel = {
	WVGA,
	32,
	16,
	COLOR_ACTIVE,
};

static struct lcd_ctrl_config lcd_cfg = {
	&disp_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data	am335x_lcd_pdata[] = {
	{
    	.manu_name              = "InnoLux",
        .controller_data        = &lcd_cfg,
       	.type                   = "3.2inch_LCD",
    },	
	{
    	.manu_name              = "InnoLux",
        .controller_data        = &lcd_cfg,
       	.type                   = "7inch_LCD",
    },	
	{
    	.manu_name              = "InnoLux",
        .controller_data        = &lcd_cfg,
       	.type                   = "8inch_LCD",
    },	
	{
    	.manu_name              = "InnoLux",
        .controller_data        = &lcd_cfg,
       	.type                   = "3.5inch_LCD",
    },	
	{
    	.manu_name              = "ADI",
        .controller_data        = &lcd_cfg,
       	.type                   = "VGA",
    },	
	{
    	.manu_name              = "InnoLux",
        .controller_data        = &lcd_cfg,
       	.type                   = "4.3inch_LCD",
    },	
};


static char lcd_type[11];
static int __init lcd_type_init(char* s) {
	
	printk("lcd_type_init %s\n", s);

        strncpy(lcd_type, s, 11);
        return 0;
}

__setup("dispmode=", lcd_type_init);



#include "common.h"

#include <linux/lis3lv02d.h>

/* TSc controller */
static struct tsc_data am335x_touchscreen_data  = {
	.wires  = 4,
	.x_plate_resistance = 200,
	.steps_to_configure = 5,
};

static struct adc_data am335x_adc_data = {
	.adc_channels = 4,
};


static struct mfd_tscadc_board tscadc = {
	.tsc_init = &am335x_touchscreen_data,
	.adc_init = &am335x_adc_data,
};


static u8 am335x_iis_serializer_direction0[] = {
	TX_MODE,	RX_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static struct snd_platform_data am335x_snd_data0 = {
	.tx_dma_offset	= 0x46000000,	/* McASP0 */
	.rx_dma_offset	= 0x46000000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction0),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction0,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt   = 1,
};


static struct omap2_hsmmc_info am335x_mmc[] __initdata = {
	{
		.mmc            = 1,
		.caps           = MMC_CAP_4_BIT_DATA,
		.gpio_cd        = GPIO_TO_PIN(3, 18),   //MCASP0_ACLKR  18
		.gpio_wp        = -EINVAL,
		.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34, /* 3V3 */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{}      /* Terminator */
};


#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	/*
	 * Setting SYSBOOT[5] should set xdma_event_intr0 pin to mode 3 thereby
	 * allowing clkout1 to be available on xdma_event_intr0.
	 * However, on some boards (like EVM-SK), SYSBOOT[5] isn't properly
	 * latched.
	 * To be extra cautious, setup the pin-mux manually.
	 * If any modules/usecase requries it in different mode, then subsequent
	 * module init call will change the mux accordingly.
	 */
	AM33XX_MUX(XDMA_EVENT_INTR0, OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT),
	AM33XX_MUX(I2C0_SDA, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	AM33XX_MUX(I2C0_SCL, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define	board_mux	NULL
#endif

/* module pin mux structure */
struct pinmux_config {
	const char *string_name; /* signal name format */
	int val; /* Options for the mux register value */
};

struct evm_dev_cfg {
	void (*device_init)(int evm_id, int profile);

/*
* If the device is required on both baseboard & daughter board (ex i2c),
* specify DEV_ON_BASEBOARD
*/
#define DEV_ON_BASEBOARD	0
#define DEV_ON_DGHTR_BRD	1
	u32 device_on;

	u32 profile;	/* Profiles (0-7) in which the module is present */
};


static struct omap_board_config_kernel am335x_evm_config[] __initdata = {
};



#define EEPROM_MAC_ADDRESS_OFFSET	60 /* 4+8+4+12+32 */
#define EEPROM_NO_OF_MAC_ADDR		3


static char am335x_mac_addr [EEPROM_NO_OF_MAC_ADDR][ETH_ALEN];

//#define AM335X_EEPROM_HEADER		0xEE3355AA

static int am33xx_evmid = -EINVAL;

/*
* am335x_evm_set_id - set up board evmid
* @evmid - evm id which needs to be configured
*
* This function is called to configure board evm id.
*/
void am335x_evm_set_id(unsigned int evmid)
{
	am33xx_evmid = evmid;
	return;
}

/*
* am335x_evm_get_id - returns Board Type (EVM/BB/EVM-SK ...)
*
* Note:
*	returns -EINVAL if Board detection hasn't happened yet.
*/
int am335x_evm_get_id(void)
{
	return am33xx_evmid;
}
EXPORT_SYMBOL(am335x_evm_get_id);


static struct pinmux_config gpio_pin_mux[] = {
/*
	{"mcasp0_ahclkx.gpio3_21",  OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"mcasp0_aclkx.gpio3_14",  	OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"mcasp0_axr0.gpio3_16",  	OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"mcasp0_axr1.gpio3_20",  	OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"mcasp0_fsr.gpio3_19",  	OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"mcasp0_fsx.gpio3_15",  	OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a1.gpio1_17",        OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a3.gpio1_19",        OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_ad8.gpio0_22",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad9.gpio0_23",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad10.gpio0_26",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad11.gpio0_27",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad12.gpio1_12",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad13.gpio1_13",		OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},     
	{"gpmc_ad14.gpio1_14",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad15.gpio1_15",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"spi0_cs1.gpio0_6", 		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
*/
	{"spi0_d1.gpio0_4", 		OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP},
	{"uart1_ctsn.gpio0_12",     OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"uart1_rtsn.gpio0_13",     OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad12.gpio1_12",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad14.gpio1_14",		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},     
//	{"uart0_ctsn.gpio1_8", 		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
//	{"uart0_rtsn.gpio1_9", 		OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	
	{NULL, 0},
};

/* Module pin mux for LCDC */
static struct pinmux_config lcdc_pin_mux[] = {
	{"lcd_data0.lcd_data0",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data1.lcd_data1",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data2.lcd_data2",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data3.lcd_data3",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data4.lcd_data4",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data5.lcd_data5",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data6.lcd_data6",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data7.lcd_data7",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data8.lcd_data8",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data9.lcd_data9",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data10.lcd_data10",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data11.lcd_data11",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data12.lcd_data12",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data13.lcd_data13",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data14.lcd_data14",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data15.lcd_data15",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_vsync.lcd_vsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_hsync.lcd_hsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_pclk.lcd_pclk",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_ac_bias_en.lcd_ac_bias_en", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* Pin mux for nand flash module */
static struct pinmux_config nand_pin_mux[] = {
	{"gpmc_ad0.gpmc_ad0",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.gpmc_ad1",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.gpmc_ad2",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.gpmc_ad3",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.gpmc_ad4",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.gpmc_ad5",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.gpmc_ad6",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad7.gpmc_ad7",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wait0.gpmc_wait0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wpn.gpmc_wpn",	  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn0.gpmc_csn0",	  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_advn_ale.gpmc_advn_ale",  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_oen_ren.gpmc_oen_ren",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_wen.gpmc_wen",     OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_ben0_cle.gpmc_ben0_cle",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{NULL, 0},
};

/* Module pin mux for SPI fash */
static struct pinmux_config spi0_pin_mux[] = {
	{"spi0_sclk.spi0_sclk", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_d0.spi0_d0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{"spi0_d1.spi0_d1", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_cs0.spi0_cs0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{NULL, 0},
};


/* Module pin mux for rmii1 */
static struct pinmux_config rmii1_pin_mux[] = {
	{"mii1_crs.rmii1_crs_dv", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.rmii1_txen", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.rmii1_txd1", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.rmii1_txd0", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_rxd1.rmii1_rxd1", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.rmii1_rxd0", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"rmii1_refclk.rmii1_refclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config rmii2_pin_mux[] = {
	{"gpmc_a9.mmc2_dat7", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a0.rmii2_txen", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{"gpmc_a4.rmii2_txd1", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{"gpmc_a5.rmii2_txd0", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{"gpmc_a10.rmii2_rxd1", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a11.rmii2_rxd0", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_col.rmii2_refclk", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};
/* Module pin mux for rgmii1 */
static struct pinmux_config rgmii1_pin_mux[] = {
	{"mii1_txen.rgmii1_tctl", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.rgmii1_rctl", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.rgmii1_td3", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd2.rgmii1_td2", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.rgmii1_td1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.rgmii1_td0", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txclk.rgmii1_tclk", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_rxclk.rgmii1_rclk", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.rgmii1_rd3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.rgmii1_rd2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.rgmii1_rd1", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.rgmii1_rd0", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for rgmii2 */
static struct pinmux_config rgmii2_pin_mux[] = {
	{"gpmc_a0.rgmii2_tctl", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a1.rgmii2_rctl", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a2.rgmii2_td3", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a3.rgmii2_td2", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a4.rgmii2_td1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a5.rgmii2_td0", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a6.rgmii2_tclk", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a7.rgmii2_rclk", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a8.rgmii2_rd3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a9.rgmii2_rd2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a10.rgmii2_rd1", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a11.rgmii2_rd0", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config i2c0_pin_mux[] = {
        {"i2c0_sda.i2c0_sda",    OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
                                        AM33XX_PULL_ENBL | AM33XX_PULL_UP | AM33XX_INPUT_EN},
        {"i2c0_scl.i2c0_scl",   OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
                                        AM33XX_PULL_ENBL | AM33XX_PULL_UP | AM33XX_INPUT_EN},
        {NULL, 0},
};	



/* Module pin mux for mcasp0 */
static struct pinmux_config mcasp0_pin_mux[] = {
        {"mcasp0_aclkx.mcasp0_aclkx", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
        {"mcasp0_fsx.mcasp0_fsx", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
		{"mcasp0_fsr.mcasp0_fsr", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
		{"mcasp0_axr0.mcasp0_axr0", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
        {"mcasp0_axr1.mcasp0_axr1", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
        {"mcasp0_ahclkx.mcasp0_ahclkx", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
        {NULL, 0},
};


/* Module pin mux for mmc0 */
static struct pinmux_config mmc0_common_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};


static struct pinmux_config mmc0_cd_only_pin_mux[] = {
	{"mcasp0_aclkr.gpio3_18", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};


/* Module pin mux for mmc1 */
static struct pinmux_config mmc1_common_pin_mux[] = {
	{"gpmc_ad3.mmc1_dat3",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.mmc1_dat2",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.mmc1_dat1",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad0.mmc1_dat0",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn1.mmc1_clk",	OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn2.mmc1_cmd",	OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config mmc1_dat4_7_pin_mux[] = {
	{"gpmc_ad7.mmc1_dat7",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.mmc1_dat6",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.mmc1_dat5",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.mmc1_dat4",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config mmc1_wp_only_pin_mux[] = {
	{"gpmc_csn0.gpio1_29",	OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config mmc1_cd_only_pin_mux[] = {
	{"gpmc_advn_ale.gpio2_2", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};


static struct pinmux_config d_can_pin_mux[] = {
        {"uart1_ctsn.d_can0_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
        {"uart1_rtsn.d_can0_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},		
        {"uart0_ctsn.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
        {"uart0_rtsn.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
        {NULL, 0},
        {NULL, 0},
};

/* Module pin mux for uart1 */
static struct pinmux_config uart1_pin_mux[] = {
        {"uart1_rxd.uart1_rxd", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
        {"uart1_txd.uart1_txd", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL},
        {NULL, 0},
};
/* Module pin mux for uart2 */


static struct pinmux_config uart2_pin_mux[] = {
	    //{"mii1_txclk.uart2_rxd", OMAP_MUX_MODE1| AM33XX_PIN_INPUT_PULLUP},
		//{"mii1_rxclk.uart2_txd", OMAP_MUX_MODE1| AM33XX_PULL_ENBL},
		{"spi0_sclk.uart2_rxd", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
		{"spi0_d0.uart2_txd", OMAP_MUX_MODE1 | AM33XX_PULL_ENBL},
		{NULL, 0},
};

/* Module pin mux for uart3 */
static struct pinmux_config uart3_pin_mux[] = {
//        {"mii1_rxd3.uart3_rxd", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
//        {"mii1_rxd2.uart3_txd", OMAP_MUX_MODE1 | AM33XX_PULL_ENBL},
		{"spi0_cs1.uart3_rxd", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
		{"ecap0_in_pwm0_out.uart3_txd", OMAP_MUX_MODE1 | AM33XX_PULL_ENBL},
        {NULL, 0},
};

/* Module pin mux for uart4 */
static struct pinmux_config uart4_pin_mux[] = {
        {"mii1_txd3.uart4_rxd", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
        {"mii1_txd2.uart4_txd", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
		{"uart0_ctsn.uart4_rxd", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
		{"uart0_rtsn.uart4_txd", OMAP_MUX_MODE1 | AM33XX_PULL_ENBL},
        {NULL, 0},
};

/* Module pin mux for uart5 */
static struct pinmux_config uart5_pin_mux[] = {
        {"mii1_col.uart5_rxd", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
        {"rmii1_refclk.uart5_txd", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL},
        {NULL, 0},
};

/* pinmux for led device */
static struct pinmux_config gpio_led_mux[] = {
	{"xdma_event_intr1.gpio0_20", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};

/*
* @pin_mux - single module pin-mux structure which defines pin-mux
*			details for all its pins.
*/
static void setup_pin_mux(struct pinmux_config *pin_mux)
{
	int i;

	for (i = 0; pin_mux->string_name != NULL; pin_mux++)
		omap_mux_init_signal(pin_mux->string_name, pin_mux->val);

}


/*
* @evm_id - evm id which needs to be configured
* @dev_cfg - single evm structure which includes
*				all module inits, pin-mux defines
* @profile - if present, else PROFILE_NONE
* @dghtr_brd_flg - Whether Daughter board is present or not
*/
static void _configure_device(int evm_id, struct evm_dev_cfg *dev_cfg,
	int profile)
{
	int i;

	am335x_evm_set_id(evm_id);

	/*
	* Only General Purpose & Industrial Auto Motro Control
	* EVM has profiles. So check if this evm has profile.
	* If not, ignore the profile comparison
	*/

	/*
	* If the device is on baseboard, directly configure it. Else (device on
	* Daughter board), check if the daughter card is detected.
	*/
	if (profile == PROFILE_NONE) {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->device_on == DEV_ON_BASEBOARD)
				dev_cfg->device_init(evm_id, profile);
		/*	else if (daughter_brd_detected == true)
				dev_cfg->device_init(evm_id, profile);*/
		}
	} else {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->profile & profile) {
				if (dev_cfg->device_on == DEV_ON_BASEBOARD)
					dev_cfg->device_init(evm_id, profile);
		/*		else if (daughter_brd_detected == true)
					dev_cfg->device_init(evm_id, profile);*/
			}
		}
	}
}


/* pinmux for usb0 drvvbus */
static struct pinmux_config usb0_pin_mux[] = {
	{"usb0_drvvbus.usb0_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* pinmux for usb1 drvvbus */
static struct pinmux_config usb1_pin_mux[] = {
	{"usb1_drvvbus.usb1_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static int __init conf_disp_pll(int rate)
{
	struct clk *disp_pll;
	int ret = -EINVAL;

	disp_pll = clk_get(NULL, "dpll_disp_ck");
	if (IS_ERR(disp_pll)) {
		pr_err("Cannot clk_get disp_pll\n");
		goto out;
	}

	ret = clk_set_rate(disp_pll, rate);
	clk_put(disp_pll);
out:
	return ret;
}

static void lcdc_init(int evm_id, int profile)
{
	struct da8xx_lcdc_platform_data *lcdc_pdata = NULL;
	setup_pin_mux(lcdc_pin_mux);
	int i;

	if (conf_disp_pll(300000000)) {
		pr_info("Failed configure display PLL, not attempting to"
				"register LCDC\n");
		return;
	}

	for(i=0; i<ARRAY_SIZE(am335x_lcd_pdata); i++) {
		if(!strcmp(lcd_type, am335x_lcd_pdata[i].type))
			lcdc_pdata = &am335x_lcd_pdata[i];
	}

	if(!lcdc_pdata) {
		pr_err("invalid type of lcd, set to default!\n");
		lcdc_pdata = &am335x_lcd_pdata[0];
	}

	if (am33xx_register_lcdc(lcdc_pdata))
		pr_info("Failed to register LCDC device\n");

	return;
}


#define AM335X_BACKLIGHT_MAX_BRIGHTNESS        100
#define AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS    100
#define AM335X_PWM_PERIOD_NANO_SECONDS        (1000000 * 5)

static struct platform_pwm_backlight_data am335x_backlight_data = {
	.pwm_id         = "ecap.2",
	.ch             = -1,
	.lth_brightness	= 21,
	.max_brightness = AM335X_BACKLIGHT_MAX_BRIGHTNESS,
	.dft_brightness = AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS,
	.pwm_period_ns  = AM335X_PWM_PERIOD_NANO_SECONDS,
};

static struct pinmux_config ecap2_pin_mux[] = {
	{"mcasp0_ahclkr.ecap2_in_pwm2_out", OMAP_MUX_MODE4 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static void enable_ecap2(int evm_id, int profile)
{
	setup_pin_mux(ecap2_pin_mux);
}

/* Setup pwm-backlight */
static struct platform_device am335x_backlight = {
	.name           = "pwm-backlight",
	.id             = -1,
	.dev		= {
		.platform_data = &am335x_backlight_data,
	},
};

static struct pwmss_platform_data  pwm_pdata[3] = {
	{
		.version = PWM_VERSION_1,
	},
	{
		.version = PWM_VERSION_1,
	},
	{
		.version = PWM_VERSION_1,
	},
};

static int __init backlight_init(void)
{
	int status = 0;
	int ecap_index = 2;


	am33xx_register_ecap(ecap_index, &pwm_pdata[ecap_index]);
	platform_device_register(&am335x_backlight);
	
	return status;
}
late_initcall(backlight_init);


static void mfd_tscadc_init(int evm_id, int profile)
{
	int err;

	err = am33xx_register_mfd_tscadc(&tscadc);
	if (err)
		pr_err("failed to register touchscreen device\n");
}

static void gpio_init(int evm_id, int profile)
{
	setup_pin_mux(gpio_pin_mux);
	return;
}

static void rgmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rgmii1_pin_mux);
	return;
}

static void rgmii2_init(int evm_id, int profile)
{
	setup_pin_mux(rgmii2_pin_mux);
	return;
}

static void rmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rmii1_pin_mux);
	return;
}

static void rmii2_init(int evm_id, int profile)
{
	setup_pin_mux(rmii2_pin_mux);
	return;
}

static void usb0_init(int evm_id, int profile)
{
	setup_pin_mux(usb0_pin_mux);
	return;
}

static void usb1_init(int evm_id, int profile)
{
	setup_pin_mux(usb1_pin_mux);
	return;
}

static void uart1_init(int evm_id, int profile)
{
    setup_pin_mux(uart1_pin_mux);
	return;
}

static void uart2_init(int evm_id, int profile)
{
    setup_pin_mux(uart2_pin_mux);
    return;
}

static void uart3_init(int evm_id, int profile)
{
    setup_pin_mux(uart3_pin_mux);
    return;
}

static void uart4_init(int evm_id, int profile)
{
    setup_pin_mux(uart4_pin_mux);
    return;
}

static void uart5_init(int evm_id, int profile)
{
    setup_pin_mux(uart5_pin_mux);
    return;
}

static void d_can_init(int evm_id, int profile)
{
	setup_pin_mux(d_can_pin_mux);
	/* Instance Zero */
	am33xx_d_can_init(0);
	am33xx_d_can_init(1);
}


/* NAND partition information */
static struct mtd_partition am335x_nand_partitions[] = {
/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "SPL",
		.offset         = 0,			/* Offset = 0x0 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup1",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x20000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup2",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x40000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup3",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x60000 */
		.size           = SZ_128K,
	},
	{
		.name           = "U-Boot",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x80000 */
		.size           = 15 * SZ_128K,
	},
	{
		.name           = "U-Boot Env",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x260000 */
		.size           = 1 * SZ_128K,
	},
	{
		.name           = "Kernel",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x280000 */
		.size           = 40 * SZ_128K,
	},
	{
		.name           = "File System",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x780000 */
		.size           = MTDPART_SIZ_FULL,
	},
};



/*
 * SPI Flash works at 80Mhz however SPI Controller works at 48MHz.
 * So setup Max speed to be less than that of Controller speed
 */
static struct spi_board_info am335x_spi0_slave_info[] = {
	{
		.modalias      = "spidev",
		.irq           = -1,
		.max_speed_hz  = 2000000,
		.bus_num       = 1,
		.chip_select   = 0,
	},
};

static struct gpmc_timings am335x_nand_timings = {
	.sync_clk = 0,

	.cs_on = 0,
	.cs_rd_off = 44,
	.cs_wr_off = 44,

	.adv_on = 6,
	.adv_rd_off = 34,
	.adv_wr_off = 44,
	.we_off = 40,
	.oe_off = 54,

	.access = 64,
	.rd_cycle = 82,
	.wr_cycle = 82,

	.wr_access = 40,
	.wr_data_mux_bus = 0,
};

static void evm_nand_init(int evm_id, int profile)
{
	struct omap_nand_platform_data *pdata;
	struct gpmc_devices_info gpmc_device[2] = {
		{ NULL, 0 },
		{ NULL, 0 },
	};

	setup_pin_mux(nand_pin_mux);
	pdata = omap_nand_init(am335x_nand_partitions,
		ARRAY_SIZE(am335x_nand_partitions), 0, 0,
		&am335x_nand_timings);
	if (!pdata)
		return;
	pdata->ecc_opt =OMAP_ECC_BCH8_CODE_HW;
	pdata->elm_used = true;
	gpmc_device[0].pdata = pdata;
	gpmc_device[0].flag = GPMC_DEVICE_NAND;

	omap_init_gpmc(gpmc_device, sizeof(gpmc_device));
	omap_init_elm();
}

static struct omap_rtc_pdata am335x_rtc_info = {
	.pm_off		= false,
	.wakeup_capable	= 0,
};

static void am335x_rtc_init(int evm_id, int profile)
{
	void __iomem *base;
	struct clk *clk;
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	char *dev_name = "am33xx-rtc";

	clk = clk_get(NULL, "rtc_fck");
	if (IS_ERR(clk)) {
		pr_err("rtc : Failed to get RTC clock\n");
		return;
	}

	if (clk_enable(clk)) {
		pr_err("rtc: Clock Enable Failed\n");
		return;
	}

	base = ioremap(AM33XX_RTC_BASE, SZ_4K);

	if (WARN_ON(!base))
		return;

	/* Unlock the rtc's registers */
	writel(0x83e70b13, base + 0x6c);
	writel(0x95a4f1e0, base + 0x70);

	/*
	 * Enable the 32K OSc
	 * TODO: Need a better way to handle this
	 * Since we want the clock to be running before mmc init
	 * we need to do it before the rtc probe happens
	 */
	writel(0x48, base + 0x54);

	iounmap(base);

	clk_disable(clk);
	clk_put(clk);

	if (omap_rev() >= AM335X_REV_ES2_0)
		am335x_rtc_info.wakeup_capable = 1;

	oh = omap_hwmod_lookup("rtc");
	if (!oh) {
		pr_err("could not look up %s\n", "rtc");
		return;
	}

	pdev = omap_device_build(dev_name, -1, oh, &am335x_rtc_info,
			sizeof(struct omap_rtc_pdata), NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for %s:%s.\n",
			dev_name, oh->name);
}

static void mcasp0_init(int evm_id, int profile)
{
	/* Configure McASP */
	setup_pin_mux(mcasp0_pin_mux);
	am335x_register_mcasp(&am335x_snd_data0, 0);

	return;
}

static void mmc1_emmc_init(int evm_id, int profile)
{
	setup_pin_mux(mmc1_common_pin_mux);
	setup_pin_mux(mmc1_dat4_7_pin_mux);

	am335x_mmc[1].mmc = 2;
	am335x_mmc[1].caps = MMC_CAP_8_BIT_DATA;
	am335x_mmc[1].gpio_cd = -EINVAL;
	am335x_mmc[1].gpio_wp = -EINVAL;
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */

	/* mmc will be initialized when //mmc0_init is called */
	return;
}



static void mmc0_init(int evm_id, int profile)
{

	setup_pin_mux(mmc0_common_pin_mux);
	setup_pin_mux(mmc0_cd_only_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}

static struct gpio_led gpio_leds[] = {
	{
		.name			= "heartbeat",
		.gpio			= GPIO_TO_PIN(0, 20),
		.default_trigger	= "heartbeat",
	},
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	},
};

static void gpio_led_init(int evm_id, int profile)
{
	int err;

	setup_pin_mux(gpio_led_mux);
	err = platform_device_register(&leds_gpio);
	if (err)
		pr_err("failed to register gpio led device\n");
}

/* setup spi0 */
static void spi0_init(int evm_id, int profile)
{
	setup_pin_mux(spi0_pin_mux);
	spi_register_board_info(am335x_spi0_slave_info,
			ARRAY_SIZE(am335x_spi0_slave_info));
	return;
}

/* Enable clkout2 */
static struct pinmux_config clkout2_pin_mux[] = {
	{"xdma_event_intr1.clkout2", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static void clkout2_enable(int evm_id, int profile)
{
	struct clk *ck_32;

	ck_32 = clk_get(NULL, "clkout2_ck");
	if (IS_ERR(ck_32)) {
		pr_err("Cannot clk_get ck_32\n");
		return;
	}

	clk_enable(ck_32);

	setup_pin_mux(clkout2_pin_mux);
}

static void sgx_init(int evm_id, int profile)
{
	if (omap3_has_sgx()) {
		am33xx_gpu_init();
	}
}


/* MC-AM335X-DK */
static struct evm_dev_cfg evm_dk_dev_cfg[] = {
//	{am335x_rtc_init, DEV_ON_BASEBOARD, PROFILE_ALL},
#ifdef NANDBOOT	
	{evm_nand_init, DEV_ON_BASEBOARD, PROFILE_ALL},
#else
	{mmc1_emmc_init, DEV_ON_BASEBOARD, PROFILE_ALL},
#endif
	{mmc0_init, 	DEV_ON_BASEBOARD, PROFILE_ALL},
	{rmii1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{rmii2_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
//	{lcdc_init, 	DEV_ON_BASEBOARD, PROFILE_ALL},
//	{enable_ecap2,	DEV_ON_BASEBOARD, PROFILE_ALL},
    {usb0_init,     DEV_ON_BASEBOARD, PROFILE_ALL},
    {usb1_init,     DEV_ON_BASEBOARD, PROFILE_ALL},	
    {uart1_init, 	DEV_ON_BASEBOARD, PROFILE_ALL},
#ifdef SPI0
	{spi0_init,     DEV_ON_BASEBOARD, PROFILE_ALL},
#else
	{uart2_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
#endif
	{uart3_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
	{uart4_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
//  {uart5_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
//	{d_can_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
//	{mcasp0_init,   DEV_ON_BASEBOARD, PROFILE_ALL},
//	{mfd_tscadc_init, DEV_ON_BASEBOARD, PROFILE_ALL},
	{gpio_led_init,  DEV_ON_BASEBOARD, PROFILE_ALL},
	{gpio_init, DEV_ON_BASEBOARD, PROFILE_ALL},
	{NULL, 0, 0},
};


static int am33xx_evm_tx_clk_dly_phy_fixup(struct phy_device *phydev)
{
	unsigned short int val = 0;

	phy_write(phydev, 0xd, 0x3);
	phy_write(phydev, 0xe, 0x805d);
	phy_write(phydev, 0xd, 0x4003);

	val = phy_read(phydev, 0xe);
	phy_write(phydev, 0xe, val & ~(1 << 8));

	phy_write(phydev, AR8051_PHY_DEBUG_ADDR_REG,
		  AR8051_DEBUG_RGMII_CLK_DLY_REG);
	phy_write(phydev, AR8051_PHY_DEBUG_DATA_REG, AR8051_RGMII_TX_CLK_DLY);

	return 0;
}

static int lan8720_phy_fixup(struct phy_device *phydev)
{
		phydev->supported &= ~(SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full);

		return 0;
}

/********** LAN8720 SMA2 rmii2_crs_dv_mode_sel *****/
#define AM33XX_CTRL_REGADDR(reg)	AM33XX_L4_WK_IO_ADDRESS(AM33XX_SCM_BASE + (reg))
#define AM33XX_CONTROL_SMA2_OFFSET  0x1320

static void setup_dk(void)
{
	unsigned int r = 0;
	pr_info("The board is MC-AM335X-GWA.\n");

	/* Starter Kit has Micro-SD slot which doesn't have Write Protect pin */
	am335x_mmc[0].gpio_wp = -EINVAL;

	/********** LAN8720 SMA2 rmii2_crs_dv_mode_sel *****/
	r = readl(AM33XX_CTRL_REGADDR(AM33XX_CONTROL_SMA2_OFFSET)); 
	writel(r|0x01, AM33XX_CTRL_REGADDR(AM33XX_CONTROL_SMA2_OFFSET));	
//	printk("CONTROL_SMA2 = %#x.\n", readl(AM33XX_CTRL_REGADDR(AM33XX_CONTROL_SMA2_OFFSET)));

	_configure_device(EVM_SK, evm_dk_dev_cfg, PROFILE_NONE);
	
	/********* LAN8720 fixup ***************************/
	am33xx_cpsw_init(AM33XX_CPSW_MODE_RMII, "0:00", "0:03");
	/* Atheros Tx Clk delay Phy fixup */
}


static void am335x_evm_setup(struct memory_accessor *mem_acc, void *context)
{
	setup_dk();

	/* SmartReflex also requires board information. */
	am33xx_sr_init();

	return;
}

/*
* Daughter board Detection.
* Every board has a ID memory (EEPROM) on board. We probe these devices at
* machine init, starting from daughter board and ending with baseboard.
* Assumptions :
*	1. probe for i2c devices are called in the order they are included in
*	   the below struct. Daughter boards eeprom are probed 1st. Baseboard
*	   eeprom probe is called last.
*/
static struct i2c_board_info __initdata am335x_i2c0_boardinfo[] = {
/*	{
		I2C_BOARD_INFO("wm8960", 0x1a),
	},*/
	{
		I2C_BOARD_INFO("rx8025", 0x32),
	},
};


static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	/*
	 * mode[0:3] = USB0PORT's mode
	 * mode[4:7] = USB1PORT's mode
	 * AM335X beta EVM has USB0 in OTG mode and USB1 in host mode.
	 */
	//.mode           = (MUSB_HOST << 4) | MUSB_OTG,
	.mode           = (MUSB_HOST << 4) | MUSB_HOST,
	.power		= 500,
	.instances	= 1,
};

static void __init am335x_evm_i2c_init(void)
{
	/* Initially assume General Purpose EVM Config */
//	am335x_evm_id = GEN_PURP_EVM;

//	evm_init_cpld();

	setup_pin_mux(i2c0_pin_mux);
	
	omap_register_i2c_bus(1, 100, am335x_i2c0_boardinfo,
				ARRAY_SIZE(am335x_i2c0_boardinfo));
}

void __iomem *am33xx_emif_base;

void __iomem * __init am33xx_get_mem_ctlr(void)
{

	am33xx_emif_base = ioremap(AM33XX_EMIF0_BASE, SZ_32K);

	if (!am33xx_emif_base)
		pr_warning("%s: Unable to map DDR2 controller",	__func__);

	return am33xx_emif_base;
}

void __iomem *am33xx_get_ram_base(void)
{
	return am33xx_emif_base;
}

void __iomem *am33xx_gpio0_base;

void __iomem *am33xx_get_gpio0_base(void)
{
	am33xx_gpio0_base = ioremap(AM33XX_GPIO0_BASE, SZ_4K);

	return am33xx_gpio0_base;
}

static struct resource am33xx_cpuidle_resources[] = {
	{
		.start		= AM33XX_EMIF0_BASE,
		.end		= AM33XX_EMIF0_BASE + SZ_32K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

/* AM33XX devices support DDR2 power down */
static struct am33xx_cpuidle_config am33xx_cpuidle_pdata = {
	.ddr2_pdown	= 1,
};

static struct platform_device am33xx_cpuidle_device = {
	.name			= "cpuidle-am33xx",
	.num_resources		= ARRAY_SIZE(am33xx_cpuidle_resources),
	.resource		= am33xx_cpuidle_resources,
	.dev = {
		.platform_data	= &am33xx_cpuidle_pdata,
	},
};

static void __init am33xx_cpuidle_init(void)
{
	int ret;

	am33xx_cpuidle_pdata.emif_base = am33xx_get_mem_ctlr();

	ret = platform_device_register(&am33xx_cpuidle_device);

	if (ret)
		pr_warning("AM33XX cpuidle registration failed\n");

}

static void __init am335x_evm_init(void)
{
	am33xx_cpuidle_init();
	am33xx_mux_init(board_mux);
	omap_serial_init();
	am335x_evm_i2c_init();
	am335x_evm_setup(NULL, NULL);
	omap_sdrc_init(NULL, NULL);
	usb_musb_init(&musb_board_data);
	omap_board_config = am335x_evm_config;
	omap_board_config_size = ARRAY_SIZE(am335x_evm_config);
	/* Create an alias for icss clock */
	if (clk_add_alias("pruss", NULL, "pruss_uart_gclk", NULL))
		pr_warn("failed to create an alias: icss_uart_gclk --> pruss\n");
	/* Create an alias for gfx/sgx clock */
	if (clk_add_alias("sgx_ck", NULL, "gfx_fclk", NULL))
		pr_warn("failed to create an alias: gfx_fclk --> sgx_ck\n");
}

static void __init am335x_evm_map_io(void)
{
	omap2_set_globals_am33xx();
	omapam33xx_map_common_io();
}

MACHINE_START(AM335XEVM, "am335xevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_early	= am33xx_init_early,
	.init_irq	= ti81xx_init_irq,
	.handle_irq     = omap3_intc_handle_irq,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END

MACHINE_START(AM335XIAEVM, "am335xiaevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_irq	= ti81xx_init_irq,
	.init_early	= am33xx_init_early,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END
