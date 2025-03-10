// license:BSD-3-Clause
// copyright-holders:hap
/******************************************************************************

Easi-Speech cartridge (R.Amy, 1987)
It has a GI SP0256A-AL2 (no XTAL)

The program adds a hook to 0xfd29, usage appears to be something like this:
n%=(number 0-511):a=usr9(n%)
or a=usr9(number)

Or a custom string:
a$="hello world":a$=usr9(a$)
or a$=usr9("string")

******************************************************************************/

#include "emu.h"
#include "easi_speech.h"


DEFINE_DEVICE_TYPE(MSX_CART_EASISPEECH, msx_cart_easispeech_device, "msx_cart_easispeech", "MSX Cartridge - Easi-Speech")


msx_cart_easispeech_device::msx_cart_easispeech_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, MSX_CART_EASISPEECH, tag, owner, clock)
	, msx_cart_interface(mconfig, *this)
	, m_speech(*this, "speech")
{
}

ROM_START(msx_cart_easispeech)
	ROM_REGION(0x10000, "speech", 0)
	ROM_LOAD("sp0256a-al2", 0x1000, 0x0800, CRC(b504ac15) SHA1(e60fcb5fa16ff3f3b69d36c7a6e955744d3feafc) )
ROM_END

const tiny_rom_entry *msx_cart_easispeech_device::device_rom_region() const
{
	return ROM_NAME(msx_cart_easispeech);
}

void msx_cart_easispeech_device::device_add_mconfig(machine_config &config)
{
	SP0256(config, m_speech, 3120000); // frequency unknown
	m_speech->add_route(ALL_OUTPUTS, ":speaker", 1.00);
}

std::error_condition msx_cart_easispeech_device::initialize_cartridge(std::string &message)
{
	if (!cart_rom_region())
	{
		message = "msx_cart_easispeech_device: Required region 'rom' was not found.";
		return image_error::INTERNAL;
	}

	if (cart_rom_region()->bytes() != 0x2000)
	{
		message = "msx_cart_easispeech_device: Region 'rom' has unsupported size.";
		return image_error::INVALIDLENGTH;
	}

	page(1)->install_rom(0x4000, 0x5fff, 0x2000, cart_rom_region()->base());
	page(2)->install_read_handler(0x8000, 0x8000, read8smo_delegate(*this, FUNC(msx_cart_easispeech_device::speech_r)));
	page(2)->install_write_handler(0x8000, 0x8000, write8smo_delegate(*this, FUNC(msx_cart_easispeech_device::speech_w)));

	return std::error_condition();
}

u8 msx_cart_easispeech_device::speech_r()
{
	return m_speech->lrq_r() << 7;
}

void msx_cart_easispeech_device::speech_w(u8 data)
{
	m_speech->ald_w(bitswap<6>(data,3,5,7,6,4,2));
}
