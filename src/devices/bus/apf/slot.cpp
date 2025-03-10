// license:BSD-3-Clause
// copyright-holders:Fabio Priuli
/***********************************************************************************************************

    APF Imagination / M-1000 cart emulation
    (through slot devices)

 ***********************************************************************************************************/

#include "emu.h"
#include "slot.h"

//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

DEFINE_DEVICE_TYPE(APF_CART_SLOT, apf_cart_slot_device, "apf_cart_slot", "APF Cartridge Slot")

//**************************************************************************
//    APF Cartridges Interface
//**************************************************************************

//-------------------------------------------------
//  device_apf_cart_interface - constructor
//-------------------------------------------------

device_apf_cart_interface::device_apf_cart_interface(const machine_config &mconfig, device_t &device) :
	device_interface(device, "apfcart"),
	m_rom(nullptr),
	m_rom_size(0)
{
}


//-------------------------------------------------
//  ~device_apf_cart_interface - destructor
//-------------------------------------------------

device_apf_cart_interface::~device_apf_cart_interface()
{
}

//-------------------------------------------------
//  rom_alloc - alloc the space for the cart
//-------------------------------------------------

void device_apf_cart_interface::rom_alloc(uint32_t size)
{
	if (m_rom == nullptr)
	{
		m_rom = device().machine().memory().region_alloc(device().subtag("^cart:rom"), size, 1, ENDIANNESS_LITTLE)->base();
		m_rom_size = size;
	}
}


//-------------------------------------------------
//  ram_alloc - alloc the space for the ram
//-------------------------------------------------

void device_apf_cart_interface::ram_alloc(uint32_t size)
{
	m_ram.resize(size);
}


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  apf_cart_slot_device - constructor
//-------------------------------------------------
apf_cart_slot_device::apf_cart_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, APF_CART_SLOT, tag, owner, clock),
	device_cartrom_image_interface(mconfig, *this),
	device_single_card_slot_interface<device_apf_cart_interface>(mconfig, *this),
	m_type(APF_STD),
	m_cart(nullptr)
{
}


//-------------------------------------------------
//  apf_cart_slot_device - destructor
//-------------------------------------------------

apf_cart_slot_device::~apf_cart_slot_device()
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void apf_cart_slot_device::device_start()
{
	m_cart = get_card_device();
}


//-------------------------------------------------
//  APF PCB
//-------------------------------------------------

struct apf_slot
{
	int                     pcb_id;
	const char              *slot_option;
};

// Here, we take the feature attribute from .xml (i.e. the PCB name) and we assign a unique ID to it
static const apf_slot slot_list[] =
{
	{ APF_STD,      "std" },
	{ APF_BASIC,    "basic" },
	{ APF_SPACEDST, "spacedst" }
};

static int apf_get_pcb_id(const char *slot)
{
	for (auto & elem : slot_list)
	{
		if (!strcmp(elem.slot_option, slot))
			return elem.pcb_id;
	}

	return 0;
}

static const char *apf_get_slot(int type)
{
	for (auto & elem : slot_list)
	{
		if (elem.pcb_id == type)
			return elem.slot_option;
	}

	return "std";
}


/*-------------------------------------------------
 call load
 -------------------------------------------------*/

std::error_condition apf_cart_slot_device::call_load()
{
	if (m_cart)
	{
		uint32_t size = !loaded_through_softlist() ? length() : get_software_region_length("rom");

		if (size > 0x3800)
		{
			osd_printf_error("%s: Image extends beyond the expected size for an APF cart\n", basename());
			return image_error::INVALIDLENGTH;
		}

		m_cart->rom_alloc(size);

		if (!loaded_through_softlist())
			fread(m_cart->get_rom_base(), size);
		else
			memcpy(m_cart->get_rom_base(), get_software_region("rom"), size);

		if (!loaded_through_softlist())
		{
			m_type = APF_STD;
			// attempt to identify Space Destroyer, which needs 1K of additional RAM
			if (size == 0x1800)
			{
				m_type = APF_SPACEDST;
				m_cart->ram_alloc(0x400);
			}
			if (size > 0x2000)
				m_type = APF_BASIC;
		}
		else
		{
			const char *pcb_name = get_feature("slot");
			if (pcb_name)
				m_type = apf_get_pcb_id(pcb_name);

			if (get_software_region("ram"))
				m_cart->ram_alloc(get_software_region_length("ram"));
		}

		//printf("Type: %s\n", apf_get_slot(m_type));

		return std::error_condition();
	}

	return std::error_condition();
}


/*-------------------------------------------------
 get default card software
 -------------------------------------------------*/

std::string apf_cart_slot_device::get_default_card_software(get_default_card_software_hook &hook) const
{
	if (hook.image_file())
	{
		uint64_t size;
		hook.image_file()->length(size); // FIXME: check error return

		// attempt to identify Space Destroyer, which needs 1K of additional RAM
		int type = APF_STD;
		if (size == 0x1800)
			type = APF_SPACEDST;
		if (size > 0x2000)
			type = APF_BASIC;

		char const *const slot_string = apf_get_slot(type);

		//printf("type: %s\n", slot_string);

		return std::string(slot_string);
	}

	return software_get_default_slot("std");
}

/*-------------------------------------------------
 read
 -------------------------------------------------*/

uint8_t apf_cart_slot_device::read_rom(offs_t offset)
{
	if (m_cart)
		return m_cart->read_rom(offset);
	else
		return 0xff;
}

/*-------------------------------------------------
 read
 -------------------------------------------------*/

uint8_t apf_cart_slot_device::extra_rom(offs_t offset)
{
	if (m_cart)
		return m_cart->extra_rom(offset);
	else
		return 0xff;
}

/*-------------------------------------------------
 read
 -------------------------------------------------*/

uint8_t apf_cart_slot_device::read_ram(offs_t offset)
{
	if (m_cart)
		return m_cart->read_ram(offset);
	else
		return 0xff;
}

/*-------------------------------------------------
 write
 -------------------------------------------------*/

void apf_cart_slot_device::write_ram(offs_t offset, uint8_t data)
{
	if (m_cart)
		m_cart->write_ram(offset, data);
}
