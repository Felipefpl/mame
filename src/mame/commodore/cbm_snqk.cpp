// license:BSD-3-Clause
// copyright-holders:Curt Coder
/***********************************************

 CBM Quickloads

 ***********************************************/

#include "emu.h"
#include "cbm_snqk.h"

/* prg file format
 * sfx file format
 * sda file format
 * 0 lsb 16bit address
 * 2 chip data */

/* p00 file format (p00 .. p63, s00 .. s63, ..)
 * 0x0000 C64File
 * 0x0007 0
 * 0x0008 Name in commodore encoding?
 * 0x0018 0 0
 * 0x001a lsb 16bit address
 * 0x001c data */


std::error_condition general_cbm_loadsnap( device_image_interface &image, address_space &space, offs_t offset,
	void (*cbm_sethiaddress)(address_space &space, uint16_t hiaddress) )
{
	char buffer[7];
	std::vector<uint8_t> data;
	uint32_t bytesread;
	uint16_t address = 0;

	int snapshot_size = image.length();

	if (image.is_filetype("prg"))
	{
		/* prg files */
	}
	else if (image.is_filetype("p00"))
	{
		/* p00 files */
		if (image.fread( buffer, sizeof(buffer)) != sizeof(buffer))
			return image_error::UNSPECIFIED;
		if (memcmp(buffer, "C64File", sizeof(buffer)))
			return image_error::INVALIDIMAGE;
		image.fseek(26, SEEK_SET);
		snapshot_size -= 26;
	}
	else if (image.is_filetype("t64"))
	{
		/* t64 files - for GB64 Single T64s loading to x0801 - header is always the same size */
		if (image.fread( buffer, sizeof(buffer)) != sizeof(buffer))
			return image_error::UNSPECIFIED;
		if (memcmp(buffer, "C64 tape image file", sizeof(buffer)))
			return image_error::INVALIDIMAGE;
		image.fseek(94, SEEK_SET);
		snapshot_size -= 94;
	}
	else
	{
		return image_error::UNSUPPORTED;
	}

	image.fread( &address, 2);
	address = little_endianize_int16(address);
	if (image.is_filetype("t64"))
		address = 2049;
	snapshot_size -= 2;

	data.resize(snapshot_size);

	bytesread = image.fread( &data[0], snapshot_size);
	if (bytesread != snapshot_size)
		return image_error::UNSPECIFIED;

	for (int i = 0; i < snapshot_size; i++)
		space.write_byte(address + i + offset, data[i]);

	cbm_sethiaddress(space, address + snapshot_size);
	return std::error_condition();
}

void cbm_quick_sethiaddress(address_space &space, uint16_t hiaddress)
{
	space.write_byte(0xae, hiaddress & 0xff);
	space.write_byte(0x31, hiaddress & 0xff);
	space.write_byte(0x2f, hiaddress & 0xff);
	space.write_byte(0x2d, hiaddress & 0xff);
	space.write_byte(0xaf, hiaddress >> 8);
	space.write_byte(0x32, hiaddress >> 8);
	space.write_byte(0x30, hiaddress >> 8);
	space.write_byte(0x2e, hiaddress >> 8);
}
