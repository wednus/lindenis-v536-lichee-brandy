/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#include <malloc.h>
#include "nand_bsp.h"
#include <sunxi_mbr.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <private_toc.h>
#include <asm/arch/nand_boot0.h>
#include "../../sprite/sprite_verify.h"

extern int NAND_UbootInit(int boot_mode);
extern int NAND_UbootToPhy(void);
extern int NAND_UbootExit(void);
extern uint NAND_UbootRead(uint start, uint sectors, void *buffer);
extern uint NAND_UbootWrite(uint start, uint sectors, void *buffer);
extern int NAND_BurnBoot0(uint length, void *buffer);
extern int NAND_BurnUboot(uint length, void *buffer);
extern int NAND_ReadBoot0(uint length, void *buffer);

extern int NAND_PhyInit(void);
extern int NAND_PhyExit(void);
extern int NAND_Uboot_Erase(int erase_flag);
extern int NAND_UbootProbe(void);
extern int NAND_GetParam_store(void *buffer, uint length);
extern int NAND_FlushCache(void);

extern PARTITION_MBR nand_mbr;
extern int  mbr_burned_flag;

static int  nand_open_times;

DECLARE_GLOBAL_DATA_PTR;

 int nand_get_mbr(char* buffer, uint len)
{
	int i,j;

	sunxi_mbr_t *mbr = (sunxi_mbr_t *)buffer;

	nand_mbr.PartCount = mbr->PartCount +1;
	nand_mbr.array[0].addr = 0;
	nand_mbr.array[0].len = mbr->array[0].addrlo;
	nand_mbr.array[0].user_type = 0x8000;
	nand_mbr.array[0].classname[0] = 'm';
	nand_mbr.array[0].classname[1] = 'b';
	nand_mbr.array[0].classname[2] = 'r';
	nand_mbr.array[0].classname[3] = '\0';

	for(i=1; i<nand_mbr.PartCount; i++)
	{
		for(j=0;j<16;j++)
			nand_mbr.array[i].classname[j] = mbr->array[i-1].name[j];

		nand_mbr.array[i].addr = nand_mbr.array[i-1].addr + nand_mbr.array[i-1].len;
		nand_mbr.array[i].len = mbr->array[i-1].lenlo;
		nand_mbr.array[i].user_type =mbr->array[i-1].user_type;
		if(i == 1)
			nand_mbr.array[0].user_type = nand_mbr.array[1].user_type;
	}

	for(j=0;j<16;j++)
		nand_mbr.array[nand_mbr.PartCount-1].classname[j] = mbr->array[nand_mbr.PartCount-2].name[j];

	nand_mbr.array[nand_mbr.PartCount-1].addr = nand_mbr.array[nand_mbr.PartCount-2].addr + nand_mbr.array[nand_mbr.PartCount-2].len;
	nand_mbr.array[nand_mbr.PartCount-1].len = 0;

//for DEBUG
	{
		printf("total part: %d\n", nand_mbr.PartCount);
		for(i=0; i<nand_mbr.PartCount; i++)
		{
			printf("%s %d, %x, %x\n",nand_mbr.array[i].classname,i, nand_mbr.array[i].len, nand_mbr.array[i].user_type);
		}
	}
	return 0;
}


int nand_uboot_init(int boot_mode)
{
	if(!nand_open_times)
	{
		nand_open_times ++;
		printf("NAND_UbootInit\n");
		return NAND_UbootInit(boot_mode);
	}
	printf("nand already init\n");
	nand_open_times ++;

    return 0;
}

int nand_uboot_init_force_sprite(int boot_mode)
{
	    nand_open_times ++;
	    printf("NAND_UbootInit\n");
	    return NAND_UbootInit(boot_mode);
}
int nand_uboot_exit(int force)
{
	if(!nand_open_times)
	{
		printf("nand not opened\n");
		return 0;
	}
	if(force)
	{
		if(nand_open_times)
		{
			nand_open_times = 0;
			printf("NAND_UbootExit\n");
			return NAND_UbootExit();
		}
	}
	printf("nand not need closed\n");

	return 0;
}

int nand_uboot_probe(void)
{
	debug("nand_uboot_probe\n");
	return NAND_UbootProbe();
}


uint nand_uboot_read(uint start, uint sectors, void *buffer)
{
	int ret;
	ret = NAND_UbootRead(start, sectors, buffer);
	if(ret<0)
		return 0;
	else
		return sectors;
}

uint nand_uboot_write(uint start, uint sectors, void *buffer)
{
	int ret;
	ret = NAND_UbootWrite(start, sectors, buffer);
	if(ret<0)
		return 0;
	else
		return sectors;
}

/*read boot0 at boot stage*/
int nand_read_boot0(void *buffer, uint length)
{
	return NAND_ReadBoot0(length, buffer);
}

int nand_verify_boot0(uint length)
{
	int ret = 0;
	char *buffer = NULL;

	debug("%s\n",  __func__);
	buffer = (char *)malloc(length);
	if (!buffer) {
		printf("%s: malloc %d byte memory fail\n",  __func__, length);
		return -1;
	}
	memset(buffer, 0, length);

	ret = nand_read_boot0(buffer, length);
	if (ret < 0) {
		printf("%s: read boot0 from nand fail\n", __func__);
		ret = 0;
		goto ERR_OUT;
	}

	if (gd->bootfile_mode  == SUNXI_BOOT_FILE_NORMAL || gd->bootfile_mode  == SUNXI_BOOT_FILE_PKG) {
		boot0_file_head_t    *boot0  = (boot0_file_head_t *)buffer;
		printf("%s\n", boot0->boot_head.magic);
		if (strncmp((const char *)boot0->boot_head.magic, BOOT0_MAGIC, MAGIC_SIZE)) {
			printf("%s : boot0 magic is error\n", __func__);
			goto ERR_OUT;
		}

		if (sunxi_sprite_verify_checksum(buffer, boot0->boot_head.length, boot0->boot_head.check_sum)) {
			printf("%s: boot0 checksum is error flash_check_sum=0x%x\n", __func__,  boot0->boot_head.check_sum);
			goto ERR_OUT;
		}
		ret = 1;
	} else {
		toc0_private_head_t  *toc0   = (toc0_private_head_t *)buffer;
		printf("%s\n", (char *)toc0->name);
		if (strncmp((const char *)toc0->name, TOC0_MAGIC, MAGIC_SIZE)) {
			printf("%s : toc0 magic is error\n", __func__);
			goto ERR_OUT;
		}

		if (sunxi_sprite_verify_checksum(buffer, toc0->length, toc0->check_sum)) {
			printf("%s : toc0 checksum is error flash_check_sum=0x%x\n", __func__,  toc0->check_sum);
			goto ERR_OUT;
		}
		ret = 1;
	}

ERR_OUT:
	if (buffer != NULL)
		free(buffer);

	if (!ret)
		return -1;
	else
		return 0;

}

int nand_download_boot0(uint length, void *buffer)
{
	int ret;

	if(!NAND_PhyInit())
	{
		ret = NAND_BurnBoot0(length, buffer);
		if (ret != 0) {
			printf("NAND_BurnBoot0 fail\n");
			return -1;
		}
		ret = nand_verify_boot0(length);
		if (ret != 0) {
			printf("nand_verify_boot0 fail\n");
			return -1;
		}
	}
	else
	{
		ret = -1;
	}
	NAND_PhyExit();
	return ret;

}

//write boot0 at boot stage
int nand_write_boot0(void *buffer,uint length)
{
	int ret = 0;
	if((length <= 0)||(buffer == NULL))
	{
		printf("%s: the para is invaild \n",__func__);
		return -1;
	}
	printf("force download boot0 or sboot \n");
	ret = NAND_BurnBoot0(length, buffer);
	return ret;
}

int nand_verify_uboot(uint length)
{
	int ret = 0;
	char *buffer = NULL;

	debug("%s\n",  __func__);
	buffer = (char *)malloc(length);
	if (!buffer) {
		printf("%s: malloc %d byte memory fail\n",  __func__, length);
		return -1;
	}
	memset(buffer, 0, length);

	ret = nand_read_uboot_data((void *)buffer, length);
	if (ret < 0) {
		printf("%s: read uboot from %d fail\n", __func__, BOOT1_START_BLK_NUM);
		ret = 0;
		goto ERR_OUT;
	}

	if (gd->bootfile_mode  == SUNXI_BOOT_FILE_NORMAL) {
		struct spare_boot_head_t    *uboot  = (struct spare_boot_head_t *)buffer;
		printf("uboot magic %s\n", uboot->boot_head.magic);
		if (strncmp((const char *)uboot->boot_head.magic, UBOOT_MAGIC, MAGIC_SIZE)) {
			printf("%s : uboot magic is error\n", __func__);
			return -1;
		}
		length = uboot->boot_head.length;
		if (sunxi_sprite_verify_checksum(buffer, uboot->boot_head.length, uboot->boot_head.check_sum)) {
			printf("%s : uboot checksum is error flash_sum=0x%x\n", __func__,  uboot->boot_head.check_sum);
			goto ERR_OUT;
		}
		ret = 1;
	} else {
		sbrom_toc1_head_info_t *toc1 = (sbrom_toc1_head_info_t *)buffer;
		if (gd->bootfile_mode  == SUNXI_BOOT_FILE_PKG) {
			printf("uboot_pkg magic 0x%x\n", toc1->magic);
		} else {
			printf("toc magic 0x%x\n", toc1->magic);
		}

		if (toc1->magic != TOC_MAIN_INFO_MAGIC) {
			printf("sunxi sprite: toc magic is error\n");
			return -1;
		}
		length = toc1->valid_len;

		if (sunxi_sprite_verify_checksum(buffer, toc1->valid_len, toc1->add_sum)) {
			printf("%s : toc1 checksum is error flash_sum=0x%x\n", __func__, toc1->add_sum);
			goto ERR_OUT;
		}
		ret = 1;
	}

ERR_OUT:
	if (buffer != NULL)
		free(buffer);

	if (!ret)
		return -1;
	else
		return 0;

}


int nand_download_uboot(uint length, void *buffer)
{
	int ret;
	debug("nand_download_uboot\n");
	if(!NAND_PhyInit())
	{
		ret = NAND_BurnUboot(length, buffer);
		if (ret != 0) {
			printf("nand burn uboot error ret = %d\n", ret);
			return -1;
		}
		ret = nand_verify_uboot(length);
		if (ret != 0) {
			printf("nand_verify_uboot fail\n");
			return -1;
		}

	}
	else
	{
		debug("nand phyinit error\n");
		ret = -1;
	}
	NAND_PhyExit();
	return ret;
}

int nand_force_download_uboot(uint length,void *buffer)
{
	int ret = 0;
	if((length <= 0)||(buffer == NULL))
	{
		printf("force download uboot error : the para is invaild \n");
		return -1;
	}
	printf("force download uboot \n");
	ret = NAND_BurnUboot(length,buffer);
	return ret ;
}

int nand_uboot_erase(int user_erase)
{
	return NAND_Uboot_Erase(user_erase);
}


uint nand_uboot_get_flash_info(void *buffer, uint length)
{
	return NAND_GetParam_store(buffer, length);
}

uint nand_uboot_set_flash_info(void *buffer, uint length)
{
	return 0;
}

uint nand_uboot_get_flash_size(void)
{
	return get_nftl_cap();
}

int nand_uboot_flush(void)
{
	return NAND_FlushCache();
}
