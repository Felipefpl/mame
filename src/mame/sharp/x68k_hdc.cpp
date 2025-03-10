// license:BSD-3-Clause
// copyright-holders:Barry Rodewald
/*

    X68000 custom SASI Hard Disk controller

 0xe96001 (R/W) - SASI data I/O
 0xe96003 (W)   - SEL signal high (0)
 0xe96003 (R)   - SASI status
                  - bit 4 = MSG - if 1, content of data line is a message
                  - bit 3 = Command / Data - if 1, content of data line is a command or status, otherwise it is data.
                  - bit 2 = I/O - if 0, Host -> Controller (Output), otherwise Controller -> Host (Input).
                  - bit 1 = BSY - if 1, HD is busy.
                  - bit 0 = REQ - if 1, host is demanding data transfer to the host.
 0xe96005 (W/O) - data is arbitrary (?)
 0xe96007 (W/O) - SEL signal low (1)

*/

#include "emu.h"
#include "x68k_hdc.h"
#include "imagedev/harddriv.h"
#include "image.h"

ALLOW_SAVE_TYPE(x68k_hdc_image_device::sasi_phase);

DEFINE_DEVICE_TYPE(X68KHDC, x68k_hdc_image_device, "x68k_hdc_image", "SASI Hard Disk")

x68k_hdc_image_device::x68k_hdc_image_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: harddisk_image_base_device(mconfig, X68KHDC, tag, owner, clock)
{
}

TIMER_CALLBACK_MEMBER(x68k_hdc_image_device::req_timer_callback)
{
	m_status_port |= SASI_STATUS_REQ;
}

void x68k_hdc_image_device::device_start()
{
	m_status = 0x00;
	m_status_port = 0x00;
	m_phase = sasi_phase::BUSFREE;
	m_req_timer = timer_alloc(FUNC(x68k_hdc_image_device::req_timer_callback), this);

	save_item(NAME(m_phase));
	save_item(NAME(m_status_port));
	save_item(NAME(m_status));
	save_item(NAME(m_command));
	save_item(NAME(m_sense));
	save_item(NAME(m_command_byte_count));
	save_item(NAME(m_command_byte_total));
	save_item(NAME(m_current_command));
	save_item(NAME(m_transfer_byte_count));
	save_item(NAME(m_transfer_byte_total));
}

std::error_condition x68k_hdc_image_device::call_create(int format_type, util::option_resolution *format_options)
{
	// create 20MB HD
	int x;
	int ret;
	unsigned char sectordata[256];  // empty block data

	memset(sectordata,0,sizeof(sectordata));
	for(x=0;x<0x013c98;x++)  // 0x13c98 = number of blocks on a 20MB HD
	{
		ret = fwrite(sectordata,256);
		if(ret < 256)
			return image_error::UNSPECIFIED;
	}

	return std::error_condition();
}

void x68k_hdc_image_device::hdc_w(offs_t offset, u16 data)
{
	unsigned int lba = 0;
	std::vector<char> blk;
	switch(offset)
	{
	case 0x00:  // data I/O
		if(m_phase == sasi_phase::WRITE)
		{
			if(m_transfer_byte_count == 0)
			{
				switch(m_command[0])
				{
				case SASI_CMD_SPECIFY:
					m_transfer_byte_total = 10;
					break;
				case SASI_CMD_WRITE:
					m_transfer_byte_total = (0x100 * m_command[4]);
					break;
				default:
					m_transfer_byte_total = 0x100;
				}
			}

			if(m_command[0] == SASI_CMD_SPECIFY)
			{
				logerror("SPECIFY: wrote 0x%02x\n",data);
			}

			if(m_command[0] == SASI_CMD_WRITE)
			{
				if(!exists())
				{
					m_phase = sasi_phase::STATUS;
					m_status_port |= SASI_STATUS_IO;  // Output (C/D remains the same)
					m_status = 0x02;
					logerror("SASI: No HD connected.\n");
				}
				else
				{
					fwrite(&data,1);
				}
			}

			m_status_port &= ~SASI_STATUS_REQ;
			m_req_timer->adjust(attotime::from_nsec(450));
			m_transfer_byte_count++;
			if(m_transfer_byte_count >= m_transfer_byte_total)
			{
				// End of transfer
				m_phase = sasi_phase::STATUS;
				m_status_port |= SASI_STATUS_IO;
				m_status_port |= SASI_STATUS_CD;
				logerror("SASI: Write transfer complete\n");
			}
		}
		if(m_phase == sasi_phase::COMMAND)
		{
			if(m_command_byte_count == 0)
			{
				// first command byte
				m_current_command = data;
				switch(data >> 5)  // high 3 bits determine command class, and therefore, length
				{
				case 0:
					m_command_byte_total = 6;
					break;
				case 1:
					m_command_byte_total = 10;
					break;
				case 2:
					m_command_byte_total = 8;
					break;
				default:
					m_command_byte_total = 6;
				}
			}
			m_command[m_command_byte_count] = data;
			// reset REQ temporarily
			m_status_port &= ~SASI_STATUS_REQ;
			m_req_timer->adjust(attotime::from_nsec(450));

			m_command_byte_count++;
			if(m_command_byte_count >= m_command_byte_total)
			{
				// End of command

				switch(m_command[0])
				{
				case SASI_CMD_REZERO_UNIT:
					m_phase = sasi_phase::STATUS;
					m_status_port |= SASI_STATUS_IO;  // Output
					// C/D remains the same
					logerror("SASI: REZERO UNIT\n");
					break;
				case SASI_CMD_REQUEST_SENSE:
					m_phase = sasi_phase::READ;
					m_status_port |= SASI_STATUS_IO;
					m_status_port &= ~SASI_STATUS_CD;
					m_transfer_byte_count = 0;
					m_transfer_byte_total = 0;
					logerror("SASI: REQUEST SENSE\n");
					break;
				case SASI_CMD_SPECIFY:
					m_phase = sasi_phase::WRITE;
					m_status_port &= ~SASI_STATUS_IO;
					m_status_port &= ~SASI_STATUS_CD;  // Data
					m_transfer_byte_count = 0;
					m_transfer_byte_total = 0;
					logerror("SASI: SPECIFY\n");
					break;
				case SASI_CMD_READ:
					if(!exists())
					{
						m_phase = sasi_phase::STATUS;
						m_status_port |= SASI_STATUS_IO;  // Output
						m_status_port |= SASI_STATUS_CD;
						m_status = 0x02;
						logerror("SASI: No HD connected\n");
					}
					else
					{
						m_phase = sasi_phase::READ;
						m_status_port |= SASI_STATUS_IO;
						m_status_port &= ~SASI_STATUS_CD;
						m_transfer_byte_count = 0;
						m_transfer_byte_total = 0;
						lba = m_command[3];
						lba |= m_command[2] << 8;
						lba |= (m_command[1] & 0x1f) << 16;
						fseek(lba * 256,SEEK_SET);
						logerror("SASI: READ (LBA 0x%06x, blocks = %i)\n",lba,m_command[4]);
					}
					break;
				case SASI_CMD_WRITE:
					if(!exists())
					{
						m_phase = sasi_phase::STATUS;
						m_status_port |= SASI_STATUS_IO;  // Output
						m_status_port |= SASI_STATUS_CD;
						m_status = 0x02;
						logerror("SASI: No HD connected\n");
					}
					else
					{
						m_phase = sasi_phase::WRITE;
						m_status_port &= ~SASI_STATUS_IO;
						m_status_port &= ~SASI_STATUS_CD;
						m_transfer_byte_count = 0;
						m_transfer_byte_total = 0;
						lba = m_command[3];
						lba |= m_command[2] << 8;
						lba |= (m_command[1] & 0x1f) << 16;
						fseek(lba * 256,SEEK_SET);
						logerror("SASI: WRITE (LBA 0x%06x, blocks = %i)\n",lba,m_command[4]);
					}
					break;
				case SASI_CMD_SEEK:
						m_phase = sasi_phase::STATUS;
						m_status_port |= SASI_STATUS_IO;  // Output
						m_status_port |= SASI_STATUS_CD;
						logerror("SASI: SEEK (LBA 0x%06x)\n",lba);
					break;
				case SASI_CMD_FORMAT_UNIT:
				case SASI_CMD_FORMAT_UNIT_06:
					/*
					    Format Unit command format  (differs from SASI spec?)
					    0 |   0x06
					    1 |   Unit number (0-7) | LBA MSB (high 5 bits)
					    2 |   LBA
					    3 |   LBA LSB
					    4 |   ??  (usually 0x01)
					    5 |   ??
					*/
						m_phase = sasi_phase::STATUS;
						m_status_port |= SASI_STATUS_IO;  // Output
						m_status_port |= SASI_STATUS_CD;
						lba = m_command[3];
						lba |= m_command[2] << 8;
						lba |= (m_command[1] & 0x1f) << 16;
						fseek(lba * 256,SEEK_SET);
						blk.resize(256*33);
						memset(&blk[0], 0, 256*33);
						// formats 33 256-byte blocks
						fwrite(&blk[0],256*33);
						logerror("SASI: FORMAT UNIT (LBA 0x%06x)\n",lba);
					break;
				default:
					m_phase = sasi_phase::STATUS;
					m_status_port |= SASI_STATUS_IO;  // Output (C/D remains the same)
					m_status = 0x02;
					logerror("SASI: Invalid or unimplemented SASI command (0x%02x) received.\n",m_command[0]);
				}
			}
		}
		break;
	case 0x01:
		if(data == 0)
		{
			if(m_phase == sasi_phase::SELECTION)
			{
				// Go to Command phase
				m_phase = sasi_phase::COMMAND;
				m_status_port |= SASI_STATUS_CD;   // data port expects a command or status
				m_command_byte_count = 0;
				m_command_byte_total = 0;
				m_req_timer->adjust(attotime::from_nsec(45));
			}
		}
		break;
	case 0x02:
		break;
	case 0x03:
		if(data != 0)
		{
			if(m_phase == sasi_phase::BUSFREE)
			{
				// Go to Selection phase
				m_phase = sasi_phase::SELECTION;
				m_status_port |= SASI_STATUS_BSY;  // HDC is now busy
			}
		}
		break;
	}

//  logerror("SASI: write to HDC, offset %04x, data %04x\n",offset,data);
}

u16 x68k_hdc_image_device::hdc_r(offs_t offset)
{
	int retval = 0xff;

	switch(offset)
	{
	case 0x00:
		if(m_phase == sasi_phase::MESSAGE)
		{
			m_phase = sasi_phase::BUSFREE;
			m_status = 0;
			m_status_port = 0;  // reset all status bits to 0
			return 0x00;
		}
		if(m_phase == sasi_phase::STATUS)
		{
			m_phase = sasi_phase::MESSAGE;
			m_status_port |= SASI_STATUS_MSG;
			// reset REQ temporarily
			m_status_port &= ~SASI_STATUS_REQ;
			m_req_timer->adjust(attotime::from_nsec(450));

			return m_status;
		}
		if(m_phase == sasi_phase::READ)
		{
			if(m_transfer_byte_count == 0)
			{
				switch(m_command[0])
				{
				case SASI_CMD_REQUEST_SENSE:
					// set up sense bytes
					m_sense[0] = 0x01;  // "No index signal"
					m_sense[1] = 0;
					m_sense[2] = 0;
					m_sense[3] = 0;
					if(m_command[3] == 0)
						m_transfer_byte_total = 4;
					else
						m_transfer_byte_total = m_command[3];
					break;
				case SASI_CMD_READ:
					m_transfer_byte_total = (0x100 * m_command[4]);
					m_transfer_byte_count = 0;
					break;
				default:
					m_transfer_byte_total = 0;
				}
			}

			switch(m_command[0])
			{
			case SASI_CMD_REQUEST_SENSE:
				retval = m_sense[m_transfer_byte_count];
				logerror("REQUEST SENSE: read value 0x%02x\n",retval);
				break;
			case SASI_CMD_READ:
				if(!exists())
				{
					m_phase = sasi_phase::STATUS;
					m_status_port |= SASI_STATUS_IO;  // Output (C/D remains the same)
					m_status = 0x02;
					logerror("SASI: No HD connected.\n");
				}
				else
				{
					unsigned char val;
					fread(&val,1);
					retval = val;
				}
				break;
			default:
				retval = 0;
			}

			m_status_port &= ~SASI_STATUS_REQ;
			m_req_timer->adjust(attotime::from_nsec(450));
			m_transfer_byte_count++;
			if(m_transfer_byte_count >= m_transfer_byte_total)
			{
				// End of transfer
				m_phase = sasi_phase::STATUS;
				m_status_port |= SASI_STATUS_IO;
				m_status_port |= SASI_STATUS_CD;
				logerror("SASI: Read transfer complete\n");
			}

			return retval;
		}
		return 0x00;
	case 0x01:
//      logerror("SASI: [%08x] read from status port, read 0x%02x\n",activecpu_get_pc(),m_status_port);
		return m_status_port;
	case 0x02:
		return 0xff;  // write-only
	case 0x03:
		return 0xff;  // write-only
	default:
		return 0xff;
	}
}
