// license:BSD-3-Clause
// copyright-holders:Lee Ward, Dirk Best, Curt Coder
/*************************************************************************

    Memotech MTX 500, MTX 512 and RS 128

**************************************************************************/

#include "emu.h"
#include "mtx.h"

#include "formats/imageutl.h"

/***************************************************************************
    READ/WRITE HANDLERS
***************************************************************************/

/*-------------------------------------------------
    mtx_strobe_r - centronics strobe
-------------------------------------------------*/

uint8_t mtx_state::mtx_strobe_r()
{
	/* set STROBE low */
	m_centronics->write_strobe(false);

	return 0xff;
}

/*-------------------------------------------------
    mtx_subpage_w - rom2 subpages
-------------------------------------------------*/

/*
    The original ROM card supported 4 8KB ROM chips. These appeared in
    ROM slot 2 in subpages 0 to 3. The subpage register starts as 0, but
    is changed by attempting to write to 0x0000-0x1fff whilst in RELCPMH=0
    mode (ie. attempting to write to the OS ROM). Videowalls could use a
    later ROM card with 4 32KB ROMs. These also appeared in ROM slot 2
    in subpages 0 to 15.
*/

void mtx_state::mtx_subpage_w(uint8_t data)
{
	if (m_extrom->exists())
	{
		if ((data * 0x2000) < m_extrom->get_rom_size())
			m_rommap_bank1->configure_entry(2, m_extrom->get_rom_base() + 0x2000 * data);
		else
			m_rommap_bank1->configure_entry(2, memregion("user2")->base() + 0x4000);
		m_rommap_bank1->set_entry(2);
	}
}

/*-------------------------------------------------
    mtx_bankswitch_w - bankswitch
-------------------------------------------------*/

/*
    There are two memory models on the MTX, the standard one and a
    CBM mode. In standard mode, the memory map is defined as:

    0x0000 - 0x1fff  OSROM
    0x2000 - 0x3fff  Paged ROM
    0x4000 - 0x7fff  Paged RAM
    0x8000 - 0xbfff  Paged RAM
    0xc000 - 0xffff  RAM

    Banks are selected by output port 0. Bits 0-3 define the RAM page
    and bits 4-6 the ROM page.

    CBM mode is selected by bit 7 of output port 0. ROM is replaced
    by RAM in this mode.
*/

void mtx_state::bankswitch(uint8_t data)
{
	/*

	    bit     description

	    0       P0
	    1       P1
	    2       P2
	    3       P3
	    4       R0
	    5       R1
	    6       R2
	    7       RELCPMH

	*/
	address_space &program = m_maincpu->space(AS_PROGRAM);

	uint8_t cbm_mode = (data >> 7) & 0x01;
	uint8_t rom_page = (data >> 4) & 0x07;
	uint8_t ram_page = (data >> 0) & 0x0f;

	if (cbm_mode)
	{
		/* ram based memory map */
		program.install_readwrite_bank(0x0000, 0x3fff, m_rammap_bank1);
		program.install_readwrite_bank(0x4000, 0x7fff, m_rammap_bank2);
		program.install_readwrite_bank(0x8000, 0xbfff, m_rammap_bank3);

		/* set ram bank, for invalid pages a nop-handler will be installed */
		if ((ram_page == 0 && m_ram->size() > 0xc000) || (ram_page > 0 && m_ram->size() > 0x4000 + ram_page * 0xc000))
			m_rammap_bank1->set_entry(ram_page);
		else
			program.nop_readwrite(0x0000, 0x3fff);

		if ((ram_page == 0 && m_ram->size() > 0x8000) || (ram_page > 0 && m_ram->size() > 0x8000 + ram_page * 0xc000))
			m_rammap_bank2->set_entry(ram_page);
		else
			program.nop_readwrite(0x4000, 0x7fff);

		if ((ram_page == 0 && m_ram->size() > 0x4000) || (ram_page > 0 && m_ram->size() > 0xc000 + ram_page * 0xc000))
			m_rammap_bank3->set_entry(ram_page);
		else
			program.nop_readwrite(0x8000, 0xbfff);
	}
	else
	{
		/* rom based memory map */
		program.install_rom(0x0000, 0x1fff, memregion("user1")->base());
		program.install_write_handler(0x0000, 0x1fff, write8smo_delegate(*this, FUNC(mtx_state::mtx_subpage_w)));
		program.install_read_bank(0x2000, 0x3fff, m_rommap_bank1);
		program.unmap_write(0x2000, 0x3fff);
		program.install_readwrite_bank(0x4000, 0x7fff, m_rommap_bank2);
		program.install_readwrite_bank(0x8000, 0xbfff, m_rommap_bank3);

		/* set rom bank (switches between basic and assembler rom or cartridges) */
		m_rommap_bank1->set_entry(rom_page);

		/* set ram bank, for invalid pages a nop-handler will be installed */
		if (m_ram->size() > 0x8000 + ram_page * 0x8000)
			m_rommap_bank2->set_entry(ram_page);
		else
			program.nop_readwrite(0x4000, 0x7fff);

		if (m_ram->size() > 0x4000 + ram_page * 0x8000)
			m_rommap_bank3->set_entry(ram_page);
		else
			program.nop_readwrite(0x8000, 0xbfff);
	}
}

void mtx_state::mtx_bankswitch_w(uint8_t data)
{
	bankswitch(data);

	m_exp_int->bankswitch(data);
	m_exp_ext->bankswitch(data);
}

/*-------------------------------------------------
    mtx_sound_strobe_r - sound strobe
-------------------------------------------------*/

uint8_t mtx_state::mtx_sound_strobe_r()
{
	m_sn->write(m_sound_latch);
	return 0xff;
}

/*-------------------------------------------------
    mtx_sound_latch_w - sound latch write
-------------------------------------------------*/

void mtx_state::mtx_sound_latch_w(uint8_t data)
{
	m_sound_latch = data;
}

/*-------------------------------------------------
    mtx_cst_w - cassette write
-------------------------------------------------*/

void mtx_state::mtx_cst_w(uint8_t data)
{
	m_cassette->output( BIT(data, 0) ? -1 : 1);
}

/*-------------------------------------------------
    mtx_cst_motor_w - cassette motor
-------------------------------------------------*/

void mtx_state::mtx_cst_motor_w(uint8_t data)
{
	/* supported in the MTX ROM */
	switch (data)
	{
	case 0xaa:
		m_cassette->change_state(CASSETTE_MOTOR_ENABLED, CASSETTE_MASK_MOTOR);
		break;
	case 0x55:
		m_cassette->change_state(CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR);
		break;
	}
}

/*-------------------------------------------------
    mtx_prt_r - centronics status
-------------------------------------------------*/

WRITE_LINE_MEMBER(mtx_state::write_centronics_busy)
{
	m_centronics_busy = state;
}

WRITE_LINE_MEMBER(mtx_state::write_centronics_fault)
{
	m_centronics_fault = state;
}

WRITE_LINE_MEMBER(mtx_state::write_centronics_perror)
{
	m_centronics_perror = state;
}

WRITE_LINE_MEMBER(mtx_state::write_centronics_select)
{
	m_centronics_select = state;
}

uint8_t mtx_state::mtx_prt_r()
{
	/*

	    bit     description

	    0       BUSY
	    1       ERROR
	    2       PE
	    3       SLCT
	    4
	    5
	    6
	    7

	*/

	uint8_t data = 0;

	/* reset STROBE to high */
	m_centronics->write_strobe( true);

	data |= m_centronics_busy << 0;
	data |= m_centronics_fault << 1;
	data |= m_centronics_perror << 2;
	data |= m_centronics_select << 3;

	return data;
}

/*-------------------------------------------------
    mtx_sense_w - keyboard sense write
-------------------------------------------------*/

void mtx_state::mtx_sense_w(uint8_t data)
{
	m_key_sense = data;
}

/*-------------------------------------------------
    mtx_key_lo_r - keyboard low read
-------------------------------------------------*/

uint8_t mtx_state::mtx_key_lo_r()
{
	uint16_t data = 0xffff;

	for (int row = 0; row < 8; row++)
	{
		if (!(m_key_sense & (1 << row)))
		{
			data &= m_keyboard[row]->read();
			if ((row == 7 && BIT(m_joysticks->read(), 0)) || (row != 7 && BIT(m_joysticks->read(), 1)))
				data &= m_joystick[row]->read();
		}
	}

	return data & 0xff;
}

/*-------------------------------------------------
    mtx_key_lo_r - keyboard high read
-------------------------------------------------*/

uint8_t mtx_state::mtx_key_hi_r()
{
	uint16_t data = 0xffff;

	for (int row = 0; row < 8; row++)
	{
		if (!(m_key_sense & (1 << row)))
		{
			data &= m_keyboard[row]->read();
			if ((row == 7 && BIT(m_joysticks->read(), 0)) || (row != 7 && BIT(m_joysticks->read(), 1)))
				data &= m_joystick[row]->read();
		}
	}

	return (m_country->read() | data) >> 8;
}

/*-------------------------------------------------
    hrx_address_w - HRX video RAM address
-------------------------------------------------*/

void mtx_state::hrx_address_w(offs_t offset, uint8_t data)
{
	if (offset)
	{
		/*

		    bit     description

		    0       A8
		    1       A9
		    2       A10
		    3
		    4
		    5       attribute memory write enable
		    6       ASCII memory write enable
		    7       cycle (0=read/1=write)

		*/
	}
	else
	{
		/*

		    bit     description

		    0       A0
		    1       A1
		    2       A2
		    3       A3
		    4       A4
		    5       A5
		    6       A6
		    7       A7

		*/
	}
}

/*-------------------------------------------------
    hrx_data_r - HRX data read
-------------------------------------------------*/

uint8_t mtx_state::hrx_data_r()
{
	return 0;
}

/*-------------------------------------------------
    hrx_data_w - HRX data write
-------------------------------------------------*/

void mtx_state::hrx_data_w(uint8_t data)
{
}

/*-------------------------------------------------
    hrx_attr_r - HRX attribute read
-------------------------------------------------*/

uint8_t mtx_state::hrx_attr_r()
{
	return 0;
}

/*-------------------------------------------------
    hrx_attr_r - HRX attribute write
-------------------------------------------------*/

void mtx_state::hrx_attr_w(uint8_t data)
{
	/*

	    bit     description

	    0
	    1
	    2
	    3
	    4
	    5
	    6
	    7

	*/
}

/***************************************************************************
    EXTENSION BOARD ROMS
***************************************************************************/

DEVICE_IMAGE_LOAD_MEMBER( mtx_state::extrom_load )
{
	uint32_t size = m_extrom->common_get_size("rom");

	if (size > 0x80000)
	{
		osd_printf_error("%s: Unsupported rom size\n", image.basename());
		return image_error::INVALIDLENGTH;
	}

	m_extrom->rom_alloc(size, GENERIC_ROM8_WIDTH, ENDIANNESS_LITTLE);
	m_extrom->common_load_rom(m_extrom->get_rom_base(), size, "rom");

	return std::error_condition();
}

/***************************************************************************
    SNAPSHOT
***************************************************************************/

// this only works for some of the files, nothing which tries to load
// more data from tape. todo: tapes which autorun after loading
SNAPSHOT_LOAD_MEMBER(mtx_state::snapshot_cb)
{
	uint64_t length = image.length();

	if (length < 18)
	{
		osd_printf_error("%s: File too short\n", image.basename());
		return image_error::INVALIDLENGTH;
	}

	if (length >= 0x10000 - 0x4000 + 18)
	{
		osd_printf_error("%s: File too long\n", image.basename());
		return image_error::INVALIDLENGTH;
	}

	auto data = std::make_unique<uint8_t []>(length);
	if (image.fread(data.get(), length) != length)
	{
		osd_printf_error("%s: Error reading file\n", image.basename());
		return image_error::UNSPECIFIED;
	}

	// verify first byte
	if (data[0] != 0xff)
		return image_error::INVALIDIMAGE;

	// get tape name
	char tape_name[16];
	memcpy(&tape_name, &data[1], 15);
	tape_name[15] = '\0';
	image.message("Loading '%s'", tape_name);

	address_space &program = m_maincpu->space(AS_PROGRAM);

	// reset memory map
	bankswitch(0);

	// start of system variables area
	uint16_t system_variables_base = pick_integer_le(data.get(), 16, 2);

	// write system variables
	uint16_t system_variables_size = 0;

	if (system_variables_base != 0)
	{
		system_variables_size = 0xfb4b - system_variables_base;
		for (int i = 0; i < system_variables_size; i++)
			program.write_byte(system_variables_base + i, data[18 + i]);
	}

	// write actual image data
	uint16_t data_size = image.length() - 18 - system_variables_size;
	for (int i = 0; i < data_size; i++)
		program.write_byte(0x4000 + i, data[18 + system_variables_size + i]);

	logerror("snapshot name = '%s', system_size = 0x%04x, data_size = 0x%04x\n", tape_name, system_variables_size, data_size);

	return std::error_condition();
}

/***************************************************************************
    QUICKLOAD
***************************************************************************/

QUICKLOAD_LOAD_MEMBER(mtx_state::quickload_cb)
{
	uint64_t length = image.length();

	if (length < 4)
	{
		osd_printf_error("%s: File too short\n", image.basename());
		return image_error::INVALIDLENGTH;
	}

	if (length >= 0x10000 - 0x4000 + 4)
	{
		osd_printf_error("%s: File too long\n", image.basename());
		return image_error::INVALIDLENGTH;
	}

	auto data = std::make_unique<uint8_t []>(length);
	if (image.fread(data.get(), length) != length)
	{
		osd_printf_error("%s: Error reading file\n", image.basename());
		return image_error::UNSPECIFIED;
	}

	uint16_t code_base = pick_integer_le(data.get(), 0, 2);
	uint16_t code_length = pick_integer_le(data.get(), 2, 2);

	if (length < code_length)
	{
		osd_printf_error("%s: File too short\n", image.basename());
		return image_error::INVALIDIMAGE;
	}

	if (code_base < 0x4000 || (code_base + code_length) >= 0x10000)
	{
		osd_printf_error("%s: Invalid code base and length\n", image.basename());
		return image_error::INVALIDIMAGE;
	}

	// reset memory map
	bankswitch(0);

	// write image data
	address_space &program = m_maincpu->space(AS_PROGRAM);
	for (int i = 0; i < code_length; i++)
		program.write_byte(code_base + i, data[4 + i]);

	m_maincpu->set_pc(code_base);

	return std::error_condition();
}

/***************************************************************************
    MACHINE INITIALIZATION
***************************************************************************/

/*-------------------------------------------------
    MACHINE_START( mtx512 )
-------------------------------------------------*/

void mtx_state::machine_start()
{
	address_space &program = m_maincpu->space(AS_PROGRAM);

	/* setup banks for rom based memory map */
	program.install_read_bank(0x2000, 0x3fff, m_rommap_bank1);
	program.install_readwrite_bank(0x4000, 0x7fff, m_rommap_bank2);
	program.install_readwrite_bank(0x8000, 0xbfff, m_rommap_bank3);

	m_rommap_bank1->configure_entries(0, 8, memregion("user2")->base(), 0x2000);
	m_rommap_bank2->configure_entry(0, m_ram->pointer() + 0x8000);
	m_rommap_bank2->configure_entries(1, 15, m_ram->pointer() + 0x10000, 0x8000);
	m_rommap_bank3->configure_entry(0, m_ram->pointer() + 0x4000);
	m_rommap_bank3->configure_entries(1, 15, m_ram->pointer() + 0xc000, 0x8000);

	/* setup banks for ram based memory map */
	program.install_readwrite_bank(0x0000, 0x3fff, m_rammap_bank1);
	program.install_readwrite_bank(0x4000, 0x7fff, m_rammap_bank2);
	program.install_readwrite_bank(0x8000, 0xbfff, m_rammap_bank3);

	m_rammap_bank1->configure_entry(0, m_ram->pointer() + 0xc000);
	m_rammap_bank1->configure_entries(1, 15, m_ram->pointer() + 0x10000, 0xc000);
	m_rammap_bank2->configure_entry(0, m_ram->pointer() + 0x8000);
	m_rammap_bank2->configure_entries(1, 15, m_ram->pointer() + 0x14000, 0xc000);
	m_rammap_bank3->configure_entry(0, m_ram->pointer() + 0x4000);
	m_rammap_bank3->configure_entries(1, 15, m_ram->pointer() + 0x18000, 0xc000);

	/* install 4000h bytes common block */
	program.install_ram(0xc000, 0xffff, m_ram->pointer());
}

void mtx_state::machine_reset()
{
	/* extension board ROMs */
	if (m_extrom->exists())
		m_rommap_bank1->configure_entry(2, m_extrom->get_rom_base());
	/* keyboard ROMs */
	if (ioport("keyboard_rom")->read())
		m_rommap_bank1->configure_entry(7, memregion("keyboard_rom")->base() + (ioport("keyboard_rom")->read() - 1) * 0x2000);

	/* bank switching */
	bankswitch(0);
}
