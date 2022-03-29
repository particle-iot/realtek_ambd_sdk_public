#include "rtl8721d.h"
#include "build_info.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/**
 * Flash: MX25R6435FZAIL0
 *
 * Page Size: 256B
 * Sector Size: 4KB
 * Block Size: 32KB and 64KB
 *
 * [commands]                   [operation time]
 * BP (Byte-Program)            40us
 * PP (Page-Program)            1.2ms
 * SE (Sector-Erase)            140ms
 * BE32K (Block-Erase 32KB)     0.5s
 * BE (Block-Erase 64KB)        1s
 * CE (Chip-Erase)              80s
 *
 */

// PC msg format:  | Command(4Byte) | data |
// MCU msg format: | Result(4Byte)  | data |
#define PC_MSG_CMD_OFFSET           0
#define PC_MSG_DATA_OFFSET          4
#define MCU_MSG_RESULT_OFFSET       0
#define MCU_MSG_DATA_OFFSET         4

#define RESULT_OK                   0x00
#define RESULT_ERROR                0x01

#define CMD_FLASH_READ_ID           0x10  // | CMD(4Byte)|
#define CMD_FLASH_ERASE_ALL         0x11  // | CMD(4Byte)|
#define CMD_FLASH_ERASE             0x12  // | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) |
#define CMD_FLASH_READ              0x13  // | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) |
#define CMD_FLASH_WRITE             0x14  // | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) | Data(NBytes)  |
#define CMD_FLASH_VERIFY            0x15  // | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) |    CRC32      |
#define CMD_READ_EFUSE_MAC          0x16  // | CMD(4Byte)|

#define DEBUG_ENABLE 0
#if DEBUG_ENABLE
#define DBG_LOG(format, ...)        DiagPrintf(format"\r\n", ##__VA_ARGS__)
#else
#define DBG_LOG(format, ...)
#endif

#define RTL872X_FLASH_SECTOR_SIZE       4096
#define RTL872X_FLASH_BLOCK_64KB_SIZE   (64 * 1024)
#define MSG_BUF_SIZE                    (16 * 1024)

#define LOGICAL_EFUSE_SIZE      1024
#define EFUSE_SUCCESS           1
#define EFUSE_FAILURE           0

#define WIFI_MAC_OFFSET         0x11A
#define WIFI_MAC_SIZE           6

volatile uint32_t pcMsgAvail = false;
volatile uint32_t mcuMsgAvail = false;
volatile uint8_t __attribute__ ((aligned (16))) pcMsgBuf[MSG_BUF_SIZE] = {};
volatile uint8_t __attribute__ ((aligned (16))) mcuMsgBuf[MSG_BUF_SIZE] = {};

void response_via_mcu_msg_buf(uint32_t result, uint8_t* data, size_t size) {
    mcuMsgBuf[MCU_MSG_RESULT_OFFSET] = result;
    if (data) {
        _memcpy((void*)&mcuMsgBuf[MCU_MSG_DATA_OFFSET], data, size);
    }
    mcuMsgAvail = true;
}

static int read_logical_efuse(uint32_t offset, uint8_t* buf, size_t size) {
    static uint8_t efuse_buf[LOGICAL_EFUSE_SIZE];

    _memset(efuse_buf, 0xff, LOGICAL_EFUSE_SIZE);

    int ret = EFUSE_LMAP_READ(efuse_buf);
    if (ret != EFUSE_SUCCESS) {
        return -1;
    }

    if (size + offset > LOGICAL_EFUSE_SIZE) {
        size = offset > LOGICAL_EFUSE_SIZE ? 0 : LOGICAL_EFUSE_SIZE - offset;
    }

    _memcpy(buf, efuse_buf + offset, size);

    return size;
}

static int wifi_get_mac_efuse_mac_address(uint8_t* dest, size_t dest_len) {
    return read_logical_efuse(WIFI_MAC_OFFSET, dest, dest_len);
}

static void disable_rsip(void) {
    if ((HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_SYS_EFUSE_SYSCFG3) & BIT_SYS_FLASH_ENCRYPT_EN) != 0) {
        uint32_t km0_system_control = HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_LP_KM0_CTRL);
        HAL_WRITE32(SYSTEM_CTRL_BASE_LP, REG_LP_KM0_CTRL, (km0_system_control & (~BIT_LSYS_PLFM_FLASH_SCE)));
    }
}

static const uint32_t 
BOOT_RAM_RODATA_SECTION crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/**
 * @brief  Computes the 32-bit CRC of a given buffer of byte data.
 * @param  pBuffer: pointer to the buffer containing the data to be computed
 * @param  BufferSize: Size of the buffer to be computed
 * @retval 32-bit CRC
 */
uint32_t BOOT_RAM_TEXT_SECTION
crc32_compute(const uint8_t *pBuffer, uint32_t bufferSize, uint32_t const *p_crc) {
    uint32_t crc = (p_crc) ? *p_crc : 0;

    crc = crc ^ ~0U;

    while (bufferSize--) {
        crc = crc32_tab[(crc ^ *pBuffer++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ ~0U;
}

void BOOT_RAM_TEXT_SECTION
RtlFlashProgram(VOID)
{
    // FIXME: Do we need to disable UART interrupt?
    InterruptDis(UART_LOG_IRQ);

    DiagPrintf("==========================================================\r\n");
    DiagPrintf("Enter Flashloader, Build Time: "UTS_VERSION"\r\n");  
    DiagPrintf("==========================================================\r\n");

    DBG_LOG("set MSG_BUF_SIZE                        %d", MSG_BUF_SIZE);
    DBG_LOG("set pc_msg_avail_mem_addr               %08p", &pcMsgAvail);
    DBG_LOG("set mcu_msg_avail_mem_addr              %08p", &mcuMsgAvail);
    DBG_LOG("set pc_msg_data_mem_addr                %08p", &pcMsgBuf);
    DBG_LOG("set mcu_msg_data_mem_addr               %08p", &mcuMsgBuf);

    // Switch SPIC clock source
    uint32_t temp = HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_LP_CLK_CTRL0);
    temp &= ~(BIT_MASK_FLASH_CLK_SEL << BIT_SHIFT_FLASH_CLK_SEL);
    temp |= BIT_SHIFT_FLASH_CLK_XTAL;
    HAL_WRITE32(SYSTEM_CTRL_BASE_LP, REG_LP_CLK_CTRL0, temp);

    // Initialize flash parameters
    DBG_LOG("Flash init start");
    FLASH_StructInit_MXIC(&flash_init_para);
    /// dummy_cycle[0] for onebit mode
    /// dummy_cycle[1] for 2 I/O mode
    /// dummy_cycle[2] for 4 I/O mode
    flash_init_para.FLASH_rd_dummy_cyle[0] = 0;
    flash_init_para.FLASH_cur_bitmode = SpicOneBitMode;

    FLASH_Init(SpicOneBitMode);

    // Print flash id
    uint8_t flash_id[3];
    FLASH_RxCmd(flash_init_para.FLASH_cmd_rd_id, 3, flash_id);
    DBG_LOG("Flash ID: 0x%02X 0x%02X", flash_id[0], flash_id[1]);

    // Switch to high performance mode
    u32 status = 0;
    u32 data = 0;
    FLASH_RxCmd(flash_init_para.FLASH_cmd_rd_status, 1, (u8*)&status);
    FLASH_RxCmd(0x15, 2, ((u8*)&status) + 1);
    data |= (BIT(9) << 8);  // L/H Switch
    FLASH_SetStatus(flash_init_para.FLASH_cmd_wr_status, 3, (u8*)&data);
    status = 0;
    FLASH_RxCmd(flash_init_para.FLASH_cmd_rd_status, 1, (u8*)&status);
    DBG_LOG("status: 0x%X", status);
    uint8_t config[2] = {};
    FLASH_RxCmd(0x15, 2, config);
    DBG_LOG("config: TB: %d, L/H: %d", config[0]&(1<<3), config[1]&(1<<1));

    // Disable RSIP as during verification step we need the raw data, not decrypted data
    disable_rsip();

    volatile uint32_t* p_cmd = (volatile uint32_t*)&pcMsgBuf[PC_MSG_CMD_OFFSET];
    volatile uint8_t* p_data = (volatile uint8_t*)&pcMsgBuf[PC_MSG_DATA_OFFSET];
    while(1) {
        while (!pcMsgAvail);

        pcMsgAvail = false;
        _memset((void*)mcuMsgBuf, 0, MSG_BUF_SIZE);
        switch (*p_cmd) {
            case CMD_FLASH_READ_ID: {
                uint8_t id = flash_id[0];
                DBG_LOG("Read Flash ID: 0x%x", flash_id[0]);
                response_via_mcu_msg_buf(RESULT_OK, &id, sizeof(id));
                break;
            }
            case CMD_FLASH_ERASE_ALL: {
                FLASH_Erase(EraseChip, 0);
                response_via_mcu_msg_buf(RESULT_OK, NULL, 0);
                DBG_LOG("Flash mass erase");
                break;
            }
            case CMD_FLASH_ERASE: {
                uint32_t* p = (uint32_t*)p_data;
                uint32_t address = p[0];
                uint32_t size = p[1];

                // Erase head sectors
                if (address % RTL872X_FLASH_BLOCK_64KB_SIZE) {
                    uint32_t empty_area_size = RTL872X_FLASH_BLOCK_64KB_SIZE - address % RTL872X_FLASH_BLOCK_64KB_SIZE;
                    uint32_t head_size = empty_area_size > size ? size : empty_area_size;
                    uint32_t head_sector_num = (head_size + RTL872X_FLASH_SECTOR_SIZE - 1) / RTL872X_FLASH_SECTOR_SIZE;
                    for (uint32_t i = 0; i < head_sector_num; i++) {
                        FLASH_Erase(EraseSector, address + RTL872X_FLASH_SECTOR_SIZE * i);
                    }
                    DBG_LOG("Flash erase head sectors, address: 0x%08x, head_sector_num: %d", address, head_sector_num);

                    address += head_size;
                    size -= head_size;
                }

                // Erase middle blocks
                if (size > RTL872X_FLASH_BLOCK_64KB_SIZE) {
                    uint32_t middle_block_num = size / RTL872X_FLASH_BLOCK_64KB_SIZE;
                    for (uint32_t i = 0; i < middle_block_num; i++) {
                        // EraseBlock: Erase specified block(64KB)
                        FLASH_Erase(EraseBlock, address + RTL872X_FLASH_BLOCK_64KB_SIZE * i);
                    }
                    DBG_LOG("Flash erase middle blocks, address: 0x%08x, middle_block_num: %d", address, middle_block_num);

                    address += middle_block_num * RTL872X_FLASH_BLOCK_64KB_SIZE;
                    size -= middle_block_num * RTL872X_FLASH_BLOCK_64KB_SIZE;
                }

                // Erase tail sectors
                if (size > 0) {
                    uint32_t tail_sector_num = (size + RTL872X_FLASH_SECTOR_SIZE - 1) / RTL872X_FLASH_SECTOR_SIZE;
                    for (uint32_t i = 0; i < tail_sector_num; i++) {
                        FLASH_Erase(EraseSector, address + RTL872X_FLASH_SECTOR_SIZE * i);
                    }

                    DBG_LOG("Flash erase tail sectors, address: 0x%08x, tail_sector_num: %d", address, tail_sector_num);
                }

                // DBG_LOG("Flash erase sector, start sector: 0x%08x, sector num: %d, sector size: %d", start_sector, number, RTL872X_FLASH_SECTOR_SIZE);
                response_via_mcu_msg_buf(RESULT_OK, NULL, 0);
                break;
            }
            case CMD_FLASH_READ: {
                // Can only read up to 16 bytes per time?
                uint32_t* p = (uint32_t*)p_data;
                uint32_t address = p[0];
                uint32_t length = p[1];
                uint8_t* p_mcu_data = (uint8_t*)&mcuMsgBuf[MCU_MSG_DATA_OFFSET];
                for (uint32_t i = 0; i < length; i += 16){
                    FLASH_RxData(0, address + i, (length - i >= 16) ? 16 : length - i, &p_mcu_data[i]);
                }

                // Manually make the response
                mcuMsgBuf[MCU_MSG_RESULT_OFFSET] = RESULT_OK;
                mcuMsgAvail = true;
                // DBG_LOG("CMD_FLASH_READ:  addr: 0x%08x, data: %02x %02x %02x %02x", address, p_mcu_data[0], p_mcu_data[1], p_mcu_data[2], p_mcu_data[3]);
                break;
            }
            case CMD_FLASH_WRITE: {
                uint32_t* p = (uint32_t*)p_data;
                uint32_t address = p[0];
                uint32_t length = p[1];
                uint8_t* data = (uint8_t*)&p[3];
                #define WRITE_LENGTH 8
                #if WRITE_LENGTH == 256
                #define flash_write_func FLASH_TxData256B
                #else
                #define flash_write_func FLASH_TxData12B
                #endif
                for (uint32_t i = 0; i < length; i += WRITE_LENGTH) {
                    // DBG_LOG("addr: 0x%08X, write size: %d, &data[i]: %p, data[i]:0x%02X", address + i, (length - i >= 8) ? 8 : (length - i), &data[i], data[i]);
                    if (length - i >= WRITE_LENGTH) {
                        flash_write_func(address + i, WRITE_LENGTH, &data[i]);
                    } else {
                        // If the tail bytes are less than WRITE_LENGTH bytes,
                        // use FLASH_TxData12B() and fill the empty area with 0xFF
                        uint32_t last_words = (length - i + 3) / 4;
                        uint32_t tmp_data[last_words];
                        _memset(tmp_data, 0xFF, last_words*4);
                        _memcpy(tmp_data, &data[i], length - i);
                        for (uint32_t j = 0; j < last_words; j++) {
                            FLASH_TxData12B(address + i + j*4, 4, (uint8_t*)&tmp_data[j]);
                        }
                    }
                }
                // uint32_t flash_data = 0;
                // FLASH_RxData(0, address, 4, &flash_data);
                // DBG_LOG("readback:  addr: 0x%08X, data: %08X", address, flash_data);
                response_via_mcu_msg_buf(RESULT_OK, NULL, 0);
                break;
            }
            case CMD_FLASH_VERIFY: {
                uint32_t* p = (uint32_t*)p_data;
                uint32_t address = p[0] + 0x08000000;
                uint32_t length = p[1];
                uint32_t crc32 = p[2];
                uint8_t* data = (uint8_t*) address;
                uint32_t result = RESULT_ERROR;
                if (crc32 == crc32_compute(data, length, NULL)) {
                    result = RESULT_OK;
                }
                DBG_LOG("CMD_FLASH_VERIFY: CRC: 0x%x vs 0x%x", crc32, crc32_compute(data, length, NULL));
                response_via_mcu_msg_buf(result, NULL, 0);
                break;
            }
            case CMD_READ_EFUSE_MAC: {
                uint8_t mac[WIFI_MAC_SIZE] = {};
                uint32_t result = RESULT_ERROR;
                if (wifi_get_mac_efuse_mac_address(mac, sizeof(mac)) == WIFI_MAC_SIZE) {
                    result = RESULT_OK;
                }
                response_via_mcu_msg_buf(result, mac, WIFI_MAC_SIZE);
                break;
            }
            default:
                DBG_LOG("Received unknown command: 0x%x", *p_cmd);
                break;
        }
    }
}
