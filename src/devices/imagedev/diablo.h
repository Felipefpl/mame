// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller
/**********************************************************
 *   DIABLO drive image to hard disk interface
 **********************************************************/

#ifndef MAME_DEVICES_IMAGEDEV_DIABLO_H
#define MAME_DEVICES_IMAGEDEV_DIABLO_H

#pragma once

#include "harddriv.h"
#include "softlist_dev.h"

#define DIABLO_TAG(id) "diablo"#id

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

// ======================> diablo_image_device

class diablo_image_device : public harddisk_image_base_device
{
public:
	typedef device_delegate<std::error_condition (device_image_interface &)> load_delegate;
	typedef device_delegate<void (device_image_interface &)> unload_delegate;

	// construction/destruction
	diablo_image_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
	virtual ~diablo_image_device();

	template <typename Object> void set_device_load(Object &&cb) { m_device_image_load = std::forward<Object>(cb); }
	template <typename Object> void set_device_unload(Object &&cb) { m_device_image_unload = std::forward<Object>(cb); }
	void set_interface(const char *interface) { m_interface = interface; }

	// image-level overrides
	virtual std::error_condition call_load() override;
	virtual std::error_condition call_create(int create_format, util::option_resolution *create_args) override;
	virtual void call_unload() override;

	virtual bool image_is_chd_type() const noexcept override { return true; }
	virtual const char *image_interface() const noexcept override { return m_interface; }
	virtual const char *file_extensions() const noexcept override { return "chd,dsk"; }
	virtual const util::option_guide &create_option_guide() const override;

	// specific implementation
	hard_disk_file *get_hard_disk_file() { return m_hard_disk_handle; }

protected:
	// device-level overrides
	virtual void device_config_complete() override;
	virtual void device_start() override;
	virtual void device_stop() override;

	// device_image_interface implementation
	virtual const software_list_loader &get_software_list_loader() const override { return rom_software_list_loader::instance(); }

	std::error_condition internal_load_dsk();

	chd_file        *m_chd;
	chd_file        m_origchd;              /* handle to the original CHD */
	chd_file        m_diffchd;              /* handle to the diff CHD */
	hard_disk_file  *m_hard_disk_handle;

	load_delegate   m_device_image_load;
	unload_delegate m_device_image_unload;
	const char *    m_interface;
};

// device type definition
DECLARE_DEVICE_TYPE(DIABLO, diablo_image_device)

#endif // MAME_DEVICES_IMAGEDEV_DIABLO_H
