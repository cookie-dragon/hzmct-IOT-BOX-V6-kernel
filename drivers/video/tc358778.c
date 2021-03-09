#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <asm/gpio.h>


/* Global (16-bit addressable) */
#define TC358768_CHIPID			0x0000
#define TC358768_SYSCTL			0x0002
#define TC358768_CONFCTL		0x0004
#define TC358768_VSDLY			0x0006
#define TC358768_DATAFMT		0x0008
#define TC358768_GPIOEN			0x000E
#define TC358768_GPIODIR		0x0010
#define TC358768_GPIOIN			0x0012
#define TC358768_GPIOOUT		0x0014
#define TC358768_PLLCTL0		0x0016
#define TC358768_PLLCTL1		0x0018
#define TC358768_CMDBYTE		0x0022
#define TC358768_PP_MISC		0x0032
#define TC358768_DSITX_DT		0x0050
#define TC358768_FIFOSTATUS		0x00F8

/* Debug (16-bit addressable) */
#define TC358768_VBUFCTRL		0x00E0
#define TC358768_DBG_WIDTH		0x00E2
#define TC358768_DBG_VBLANK		0x00E4
#define TC358768_DBG_DATA		0x00E8

/* TX PHY (32-bit addressable) */
#define TC358768_CLW_DPHYCONTTX		0x0100
#define TC358768_D0W_DPHYCONTTX		0x0104
#define TC358768_D1W_DPHYCONTTX		0x0108
#define TC358768_D2W_DPHYCONTTX		0x010C
#define TC358768_D3W_DPHYCONTTX		0x0110
#define TC358768_CLW_CNTRL		0x0140
#define TC358768_D0W_CNTRL		0x0144
#define TC358768_D1W_CNTRL		0x0148
#define TC358768_D2W_CNTRL		0x014C
#define TC358768_D3W_CNTRL		0x0150

/* TX PPI (32-bit addressable) */
#define TC358768_STARTCNTRL		0x0204
#define TC358768_DSITXSTATUS	0x0208
#define TC358768_LINEINITCNT	0x0210
#define TC358768_LPTXTIMECNT	0x0214
#define TC358768_TCLK_HEADERCNT	0x0218
#define TC358768_TCLK_TRAILCNT	0x021C
#define TC358768_THS_HEADERCNT	0x0220
#define TC358768_TWAKEUP		0x0224
#define TC358768_TCLK_POSTCNT	0x0228
#define TC358768_THS_TRAILCNT	0x022C
#define TC358768_HSTXVREGCNT	0x0230
#define TC358768_HSTXVREGEN		0x0234
#define TC358768_TXOPTIONCNTRL	0x0238
#define TC358768_BTACNTRL1		0x023C

/* TX CTRL (32-bit addressable) */
#define TC358768_DSI_STATUS		0x0410
#define TC358768_DSI_INT		0x0414
#define TC358768_DSICMD_RXFIFO	0x0430
#define TC358768_DSI_ACKERR		0x0434
#define TC358768_DSI_RXERR		0x0440
#define TC358768_DSI_ERR		0x044C
#define TC358768_DSI_CONFW		0x0500
#define TC358768_DSI_RESET		0x0504
#define TC358768_DSI_INT_CLR	0x050C
#define TC358768_DSI_START		0x0518

/* DSITX CTRL (16-bit addressable) */
#define TC358768_DSICMD_TX		0x0600
#define TC358768_DSICMD_TYPE	0x0602
#define TC358768_DSICMD_WC		0x0604
#define TC358768_DSICMD_WD0		0x0610
#define TC358768_DSICMD_WD1		0x0612
#define TC358768_DSICMD_WD2		0x0614
#define TC358768_DSICMD_WD3		0x0616
#define TC358768_DSI_EVENT		0x0620
#define TC358768_DSI_VSW		0x0622
#define TC358768_DSI_VBPR		0x0624
#define TC358768_DSI_VACT		0x0626
#define TC358768_DSI_HSW		0x0628
#define TC358768_DSI_HBPR		0x062A
#define TC358768_DSI_HACT		0x062C

static const struct i2c_device_id tc358778_id[] = {
	{ "tc358778", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, tc358778_id);

static int tc358778_write_reg(struct i2c_client *client, int number, u8 value)
{
	int ret = i2c_smbus_write_byte_data(client, number, value);
	if (ret)
		dev_err(&client->dev, "Unable to write register #%d\n",
			number);

	return ret;
}

int Tc358778CmdSend(struct i2c_client *client, unsigned int addr, unsigned int data)
{
	int ret = -1;
	unsigned char cmd = addr >> 8;
	unsigned char databuf[3] = {0};

	
	databuf[0] = (unsigned char)addr;
	databuf[2] = (unsigned char)data;
	databuf[1] = (unsigned char)(data >> 8);

	printk("addr = %#x, data = %#x; cmd = %#x, databuf = %#x,%#x,%#x\n", addr, data, cmd, databuf[0], databuf[1], databuf[2]);
	ret = i2c_smbus_write_i2c_block_data(client, cmd, 3, databuf);
	if(ret){
		printk("Unable to write short cmd\n");
		return -1;
	}
	
	return 0;
}


int Tc358778DCSShortPakSend(struct i2c_client *client, unsigned int data)
{

	int ret = -1;
	ret = Tc358778CmdSend(client, TC358768_DSICMD_TYPE, 0x1015); //(Short packet, Data ID = 0x15)
	if(ret){
		printk("Unable to write short cmd type\n");
		return -1;
	}
	ret = Tc358778CmdSend(client, TC358768_DSICMD_WC, 0x0000); //(WC1, WC0=0 for DSC short write)
	if(ret){
		printk("Unable to set WC type\n");
		return -1;
	}
	ret = Tc358778CmdSend(client, TC358768_DSICMD_WD0, data); //(WD0,data)
	if(ret){
		printk("Unable to send WD0 data\n");
		return -1;
	}	
	ret = Tc358778CmdSend(client, TC358768_DSICMD_TX, 0x0001); //(Start transfer)
	if(ret){
		printk("Unable to Start transfer\n");
		return -1;
	}

	return 0;
}

int Tc358778DCSLongPakSend(struct i2c_client *client, unsigned int wd0, unsigned int wd1, unsigned int wd2, unsigned int wd3)
{

	int ret = -1;
	ret = Tc358778CmdSend(client, TC358768_DSICMD_TYPE, 0x4029); //(DSI Long Command/Packet, Data ID = 0x29)
	if(ret){
		printk("Unable to write short cmd type\n");
		return -1;
	}
	ret = Tc358778CmdSend(client, TC358768_DSICMD_WC, 0x0004); //(WC1, WC0=0 for DSC long write)
	if(ret){
		printk("Unable to set WC type\n");
		return -1;
	}
	if(!wd0){
		ret = Tc358778CmdSend(client, TC358768_DSICMD_WD0, wd0); //(WD0,data)
		if(ret){
			printk("Unable to send WD0 data\n");
			return -1;
		}
	}
	if(!wd1){
		ret = Tc358778CmdSend(client, TC358768_DSICMD_WD1, wd1); //(WD1,data)
		if(ret){
			printk("Unable to send WD1 data\n");
			return -1;
		}
	}
	if(!wd2){
		ret = Tc358778CmdSend(client, TC358768_DSICMD_WD2, wd2); //(WD1,data)
		if(ret){
			printk("Unable to send WD2 data\n");
			return -1;
		}
	}
	if(!wd3){
		ret = Tc358778CmdSend(client, TC358768_DSICMD_WD3, wd3); //(WD1,data)
		if(ret){
			printk("Unable to send WD3 data\n");
			return -1;
		}
	}
	ret = Tc358778CmdSend(client, TC358768_DSICMD_TX, 0x0001); //(Start transfer)
	if(ret){
		printk("Unable to Start transfer\n");
		return -1;
	}

	return 0;
}


int Tc358778LCDInit(struct i2c_client *client)
{
	// TC358768XBG Software Reset
	Tc358778CmdSend(client, TC358768_SYSCTL, 0x0001); //SYSctl, S/W Reset
	mdelay(1);
	Tc358778CmdSend(client, TC358768_SYSCTL, 0x0000); //SYSctl, S/W Reset release

	
	// TC358768XBG PLL,Clock Setting
	// *************************************************
	Tc358778CmdSend(client, TC358768_PLLCTL0, 0x2063); //PLL Control Register 0 (PLL_PRD,PLL_FBD)
	Tc358778CmdSend(client, TC358768_PLLCTL1, 0x0203); //PLL_FRS,PLL_LBWS, PLL oscillation enable
	mdelay(1);
	Tc358778CmdSend(client, TC358768_PLLCTL1, 0x0213); //PLL_FRS,PLL_LBWS, PLL clock out enable

	
	// *************************************************
	// TC358768XBG  VSDly[9:0] 
	// *************************************************
	Tc358778CmdSend(client, TC358768_VSDLY, 0x01F4);
	// **************************************************
	// TC358768XBG D-PHY Setting
	// **************************************************
	Tc358778CmdSend(client, TC358768_CLW_CNTRL, 0x0000);//D-PHY Clock lane enable
	Tc358778CmdSend(client, 0x0142, 0x0000);//
	Tc358778CmdSend(client, TC358768_D0W_CNTRL, 0x0000);//D-PHY Data lane0 enable
	Tc358778CmdSend(client, 0x0146, 0x0000);//
	Tc358778CmdSend(client, TC358768_D1W_CNTRL, 0x0000);//D-PHY Data lane1 enable
	Tc358778CmdSend(client, 0x014A, 0x0000);//
	Tc358778CmdSend(client, TC358768_D2W_CNTRL, 0x0000);//D-PHY Data lane2 enable
	Tc358778CmdSend(client, 0x014E, 0x0000);//
	Tc358778CmdSend(client, TC358768_D3W_CNTRL, 0x0000);//D-PHY Data lane3 enable
	Tc358778CmdSend(client, 0x0152, 0x0000);//
	
	Tc358778CmdSend(client, TC358768_CLW_DPHYCONTTX, 0x0002);//D-PHY Clock lane control
	Tc358778CmdSend(client, 0x0102, 0x0000);//
	Tc358778CmdSend(client, TC358768_D0W_DPHYCONTTX, 0x0002);//D-PHY Data lane0 control
	Tc358778CmdSend(client, 0x0106, 0x0000);//
	Tc358778CmdSend(client, TC358768_D1W_DPHYCONTTX, 0x0002);//D-PHY Data lane1 control
	Tc358778CmdSend(client, 0x010A, 0x0000);//
	Tc358778CmdSend(client, TC358768_D2W_DPHYCONTTX, 0x0002);//D-PHY Data lane2 control
	Tc358778CmdSend(client, 0x010E, 0x0000);//
	Tc358778CmdSend(client, TC358768_D3W_DPHYCONTTX, 0x0002);//D-PHY Data lane3 control
	Tc358778CmdSend(client, 0x0112, 0x0000);//

	// TC358768XBG DSI-TX PPI Control
	Tc358778CmdSend(client, TC358768_LINEINITCNT, 0x1644);//LINEINITCNT
	Tc358778CmdSend(client, 0x0212, 0x0000);//
	Tc358778CmdSend(client, TC358768_LPTXTIMECNT, 0x0004);//LPTXTIMECNT
	Tc358778CmdSend(client, 0x0216, 0x0000);//
	Tc358778CmdSend(client, TC358768_TCLK_HEADERCNT, 0x2004);//TCLK_HEADERCNT
	Tc358778CmdSend(client, 0x021A, 0x0000);//
	Tc358778CmdSend(client, TC358768_THS_HEADERCNT, 0x0604);//THS_HEADERCNT
	Tc358778CmdSend(client, 0x0222, 0x0000);//
	Tc358778CmdSend(client, TC358768_TWAKEUP, 0x5208);//TWAKEUPCNT
	Tc358778CmdSend(client, 0x0226, 0x0000);//
	Tc358778CmdSend(client, TC358768_THS_TRAILCNT, 0x0003);//THS_TRAILCNT
	Tc358778CmdSend(client, 0x022E, 0x0000);//
	Tc358778CmdSend(client, TC358768_HSTXVREGCNT, 0x0005);//HSTXVREGCNT
	Tc358778CmdSend(client, 0x0232, 0x0000);//
	Tc358778CmdSend(client, TC358768_HSTXVREGEN, 0x001F);//HSTXVREGEN enable
	Tc358778CmdSend(client, 0x0236, 0x0000);//
	Tc358778CmdSend(client, TC358768_TXOPTIONCNTRL, 0x0001);//DSI clock Enable/Disable during LP
	Tc358778CmdSend(client, 0x023A, 0x0000);//
	Tc358778CmdSend(client, TC358768_BTACNTRL1, 0x0005);//BTACNTRL1
	Tc358778CmdSend(client, 0x023E, 0x0004);//
	Tc358778CmdSend(client, TC358768_STARTCNTRL, 0x0001);//STARTCNTRL
	Tc358778CmdSend(client, 0x0206, 0x0000);//


	// TC358768XBG DSI-TX Timing Control
	Tc358778CmdSend(client, TC358768_DSI_EVENT, 0x0001);//Sync Pulse/Sync Event mode setting
	Tc358778CmdSend(client, TC358768_DSI_VSW, 0x0032);//V Control Register1
	Tc358778CmdSend(client, TC358768_DSI_VBPR, 0x001E);//V Control Register2
	Tc358778CmdSend(client, TC358768_DSI_VACT, 0x0500);//V Control Register3
	Tc358778CmdSend(client, TC358768_DSI_HSW, 0x0199);//H Control Register1
	Tc358778CmdSend(client, TC358768_DSI_HBPR, 0x00CD);//H Control Register2
	Tc358778CmdSend(client, TC358768_DSI_HACT, 0x03C0);//H Control Register3
	Tc358778CmdSend(client, TC358768_DSI_START, 0x0001);//DSI Start
	Tc358778CmdSend(client, 0x051A, 0x0000);//

	// LCDD (Peripheral) Setting
	Tc358778DCSLongPakSend(client, 0xFFFF, 0x0698, 0x0104, 0); // Change to Page 1
	Tc358778DCSShortPakSend(client, 0x1808);// output SDA
	Tc358778DCSShortPakSend(client, 0x0121);// DE = 1 Active  Display Function Control
	Tc358778DCSShortPakSend(client, 0x0130);// 480 X 854
	Tc358778DCSShortPakSend(client, 0x0031);// Column  inversion
	Tc358778DCSShortPakSend(client, 0x8750);// VGMP
	Tc358778DCSShortPakSend(client, 0x8751);// VGMN
	Tc358778DCSShortPakSend(client, 0x0760);// SDTI
	Tc358778DCSShortPakSend(client, 0x0061);// CRTI
	Tc358778DCSShortPakSend(client, 0x0762);// EQTI
	Tc358778DCSShortPakSend(client, 0x0063);// PCTI
	Tc358778DCSShortPakSend(client, 0x1540);// VGH/VGL
	Tc358778DCSShortPakSend(client, 0x5541);// DDVDH/DDVDL	Clamp
	Tc358778DCSShortPakSend(client, 0x0342);// VGH/VGL
	Tc358778DCSShortPakSend(client, 0x8a43);// VGH Clamp 16V
	Tc358778DCSShortPakSend(client, 0x8644);// VGL Clamp -10V
	Tc358778DCSShortPakSend(client, 0x5546);
	Tc358778DCSShortPakSend(client, 0x0023);
	
	Tc358778DCSShortPakSend(client, 0x0753); //Flicker 10min-02 3min-7
	
	//++++++++++++++++++ Gamma Setting ++++++++++++++++++//
	Tc358778DCSLongPakSend(client, 0xFFFF, 0x0698, 0x0104, 0);// Change to Page 1
    Tc358778DCSShortPakSend(client, 0x00A0); // Gamma 0
	Tc358778DCSShortPakSend(client, 0x10A1);// Gamma 4
	Tc358778DCSShortPakSend(client, 0x1AA2);// Gamma 8
	Tc358778DCSShortPakSend(client, 0x0FA3);// Gamma 16
	Tc358778DCSShortPakSend(client, 0x08A4);// Gamma 24
	Tc358778DCSShortPakSend(client, 0x0EA5);// Gamma 52
	Tc358778DCSShortPakSend(client, 0x08A6);// Gamma 80
	Tc358778DCSShortPakSend(client, 0x06A7);// Gamma 108
	Tc358778DCSShortPakSend(client, 0x08A8);// Gamma 147
	Tc358778DCSShortPakSend(client, 0x0BA9);// Gamma 175
	Tc358778DCSShortPakSend(client, 0x10AA);// Gamma 203
	Tc358778DCSShortPakSend(client, 0x07AB);// Gamma 231
	Tc358778DCSShortPakSend(client, 0x0DAC);// Gamma 239
	Tc358778DCSShortPakSend(client, 0x12AD);// Gamma 247
	Tc358778DCSShortPakSend(client, 0x0BAE);// Gamma 251
	Tc358778DCSShortPakSend(client, 0x07AF);// Gamma 255
	///==============Nagitive
	Tc358778DCSShortPakSend(client, 0x04C0);// Gamma 0	255
	Tc358778DCSShortPakSend(client, 0x10C1);// Gamma 4	251
	Tc358778DCSShortPakSend(client, 0x1AC2);// Gamma 8	247
	Tc358778DCSShortPakSend(client, 0x0FC3);// Gamma 16	239
	Tc358778DCSShortPakSend(client, 0x08C4);// Gamma 24	231
	Tc358778DCSShortPakSend(client, 0x0EC5);// Gamma 52	203
	Tc358778DCSShortPakSend(client, 0x08C6);// Gamma 80	175
	Tc358778DCSShortPakSend(client, 0x06C7);// Gamma 108	147
	Tc358778DCSShortPakSend(client, 0x08C8);// Gamma 147	108
	Tc358778DCSShortPakSend(client, 0x06C9);// Gamma 175	80
	Tc358778DCSShortPakSend(client, 0x10CA);// Gamma 203	52
	Tc358778DCSShortPakSend(client, 0x07CB);// Gamma 231	24
	Tc358778DCSShortPakSend(client, 0x0DCC);// Gamma 239	16
	Tc358778DCSShortPakSend(client, 0x12CD);// Gamma 247	8
	Tc358778DCSShortPakSend(client, 0x0BCE);// Gamma 251	4
	Tc358778DCSShortPakSend(client, 0x00CF);// Gamma 255	0
	//*************************************************************************
	//****************************** Page 6 Command ***************************
	//*************************************************************************
	Tc358778DCSLongPakSend(client, 0xFFFF, 0x0698, 0x0604, 0);// Change to Page 6
	Tc358778DCSShortPakSend(client, 0x2100);
	Tc358778DCSShortPakSend(client, 0x0601);
	Tc358778DCSShortPakSend(client, 0xA002);
	Tc358778DCSShortPakSend(client, 0x0203);
	Tc358778DCSShortPakSend(client, 0x0104);
	Tc358778DCSShortPakSend(client, 0x0105);
	Tc358778DCSShortPakSend(client, 0x8006);
	Tc358778DCSShortPakSend(client, 0x0407);
	Tc358778DCSShortPakSend(client, 0x0008);
	Tc358778DCSShortPakSend(client, 0x8009);
	Tc358778DCSShortPakSend(client, 0x000A);
	Tc358778DCSShortPakSend(client, 0x000B);
	Tc358778DCSShortPakSend(client, 0x330C);
	Tc358778DCSShortPakSend(client, 0x330D);
	Tc358778DCSShortPakSend(client, 0x090E);
	Tc358778DCSShortPakSend(client, 0x000F);
	Tc358778DCSShortPakSend(client, 0xFF10);
	Tc358778DCSShortPakSend(client, 0xF011);
	Tc358778DCSShortPakSend(client, 0x0012);
	Tc358778DCSShortPakSend(client, 0x0013);
	Tc358778DCSShortPakSend(client, 0x0014);
	Tc358778DCSShortPakSend(client, 0xC015);
	Tc358778DCSShortPakSend(client, 0x0816);
	Tc358778DCSShortPakSend(client, 0x0017);
	Tc358778DCSShortPakSend(client, 0x0018);
	Tc358778DCSShortPakSend(client, 0x0019);
	Tc358778DCSShortPakSend(client, 0x001A);
	Tc358778DCSShortPakSend(client, 0x001B);
	Tc358778DCSShortPakSend(client, 0x001C);
	Tc358778DCSShortPakSend(client, 0x001D);
	Tc358778DCSShortPakSend(client, 0x0120);
	Tc358778DCSShortPakSend(client, 0x2321);
	Tc358778DCSShortPakSend(client, 0x4522);
	Tc358778DCSShortPakSend(client, 0x6723);
	Tc358778DCSShortPakSend(client, 0x0124);
	Tc358778DCSShortPakSend(client, 0x2325);
	Tc358778DCSShortPakSend(client, 0x4526);
	Tc358778DCSShortPakSend(client, 0x6727);
	Tc358778DCSShortPakSend(client, 0x1230);
	Tc358778DCSShortPakSend(client, 0x2231);
	Tc358778DCSShortPakSend(client, 0x2232);
	Tc358778DCSShortPakSend(client, 0x2233);
	Tc358778DCSShortPakSend(client, 0x8734);
	Tc358778DCSShortPakSend(client, 0x9635);
	Tc358778DCSShortPakSend(client, 0xAA36);
	Tc358778DCSShortPakSend(client, 0xDB37);
	Tc358778DCSShortPakSend(client, 0xCC38);
	Tc358778DCSShortPakSend(client, 0xBD39);
	Tc358778DCSShortPakSend(client, 0x783A);
	Tc358778DCSShortPakSend(client, 0x693B);
	Tc358778DCSShortPakSend(client, 0x223C);
	Tc358778DCSShortPakSend(client, 0x223D);
	Tc358778DCSShortPakSend(client, 0x223E);
	Tc358778DCSShortPakSend(client, 0x223F);
	Tc358778DCSShortPakSend(client, 0x2240);
	//*************************************************************************
	Tc358778DCSLongPakSend(client, 0xFFFF, 0x0698, 0x0704, 0);// Change to Page 7
	Tc358778DCSShortPakSend(client, 0x1306);
	Tc358778DCSShortPakSend(client, 0x7702);
	Tc358778DCSShortPakSend(client, 0x1D18);//VREG1/2OUT ENABLE
	//*************************************************************************
	Tc358778DCSLongPakSend(client, 0xFFFF, 0x0698, 0x0004, 0);// Change to Page 0
	Tc358778DCSShortPakSend(client, 0x0036);
	Tc358778DCSShortPakSend(client, 0x0011);// Sleep-Out
	Tc358778DCSShortPakSend(client, 0x0029);// Display On


	// **************************************************
	// Set to HS mode
	// **************************************************
	Tc358778CmdSend(client,TC358768_DSI_CONFW, 0x0086);//DSI lane setting, DSI mode=HS
	Tc358778CmdSend(client,0x0502, 0xA300);//bit set
	Tc358778CmdSend(client,TC358768_DSI_CONFW, 0x8000);//Switch to DSI mode
	Tc358778CmdSend(client,0x0502, 0xC300);//
	// **************************************************
	// Host: RGB(DPI) input start
	// **************************************************
	Tc358778CmdSend(client,TC358768_DATAFMT, 0x0057);//DSI-TX Format setting
	Tc358778CmdSend(client,TC358768_DSITX_DT, 0x000E);//DSI-TX Pixel stream packet Data Type setting
	Tc358778CmdSend(client,TC358768_PP_MISC,0x0000);//HSYNC Polarity
	Tc358778CmdSend(client,TC358768_CONFCTL,0x0244);//Configuration Control Register
}



#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + gpio)

static int __devinit tc358778_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	gpio_request(GPIO_TO_PIN(1, 13), "backlight");
	gpio_direction_output(GPIO_TO_PIN(1, 13), 1);
	printk("tc358778 probe ************************\n");

	Tc358778LCDInit(client);
	return 0;
}

static int __devexit tc358778_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver tc358778_driver = {
    .driver = {
        .name = "tc358778",
        .owner = THIS_MODULE,
    },
    .probe      = tc358778_probe,
    .remove     = __devexit_p(tc358778_remove),
    .id_table   = tc358778_id,
};

static int __init tc358778_init(void)
{
	i2c_add_driver(&tc358778_driver);
}

static void __exit tc358778_exit(void)
{
	i2c_del_driver(&tc358778_driver);
}

MODULE_AUTHOR("hzmct");
MODULE_DESCRIPTION("tc358778 driver");
MODULE_LICENSE("GPL");

module_init(tc358778_init);
module_exit(tc358778_exit);
