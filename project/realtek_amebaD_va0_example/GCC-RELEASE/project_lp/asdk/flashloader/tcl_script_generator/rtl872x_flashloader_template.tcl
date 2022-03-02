#
# OpenOCD script for RTL872x
#

transport select swd
adapter speed 4000

source [find target/swj-dp.tcl]

if { [info exists CHIPNAME] } {
    set _CHIPNAME $CHIPNAME
} else {
    set _CHIPNAME rtl872x
}

if { [info exists ENDIAN] } {
    set _ENDIAN $ENDIAN
} else {
    set _ENDIAN little
}

if { [info exists WORKAREASIZE] } {
    set _WORKAREASIZE $WORKAREASIZE
} else {
    set _WORKAREASIZE 0x800
}

if { [info exists CPUTAPID] } {
    set _CPUTAPID $CPUTAPID
} else {
    set _CPUTAPID 0x6ba02477
}

swj_newdap $_CHIPNAME cpu -irlen 4 -expected-id $_CPUTAPID
dap create $_CHIPNAME.dap -chain-position $_CHIPNAME.cpu

set _TARGETNAME $_CHIPNAME.cpu
target create $_TARGETNAME cortex_m -endian $_ENDIAN -dap $_CHIPNAME.dap

if {![using_hla]} {
    cortex_m reset_config sysresetreq
}

#
# Flash loader for RTL872x
#
# PC msg format:  | Command(4Byte) | data |
# MCU msg format: | Result(4Byte)  | data |
#
set PC_MSG_CMD_OFFSET                   0
set PC_MSG_DATA_OFFSET                  4
set MCU_MSG_RESULT_OFFSET               0
set MCU_MSG_DATA_OFFSET                 4

set RESULT_OK                           0x00
set RESULT_ERROR                        0x01

set CMD_FLASH_READ_ID                   0x10  ;# | CMD(4Byte)|
set CMD_FLASH_ERASE_ALL                 0x11  ;# | CMD(4Byte)|
set CMD_FLASH_ERASE                     0x12  ;# | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) |
set CMD_FLASH_READ                      0x13  ;# | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) |
set CMD_FLASH_WRITE                     0x14  ;# | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) | reserved  | Data(NBytes)  |
set CMD_FLASH_VERIFY                    0x15  ;# | CMD(4Byte)|    Address(4Bytes)    | Length(4Bytes) |   CRC32   |
set CMD_READ_EFUSE_MAC                  0x16  ;# | CMD(4Byte)|

set WIFI_MAC_SIZE                       6

set RTL872x_REG_PERI_ON_BASE            0x48000000
set RTL872x_REG_BOOT_CFG                0x480003F8
set RTL872x_REG_LP_KM4_CTRL             0x4800021C  ; # bit3 = 0, bit24 = 0
set RTL872x_REG_AON_PM_OPT              0x48000054  ; # Enable HW reset when download finish, bit1 = 1

set FLASH_SECTOR_SIZE                   4096
set FLASH_BLOCK_SIZE                    [expr 64 * 1024]
set FLASH_MMU_START_ADDR                0x08000000

set MSG_BUF_SIZE                        16384
set pc_msg_avail_mem_addr               0x00087410
set mcu_msg_avail_mem_addr              0x00083400
set pc_msg_data_mem_addr                0x00087420
set mcu_msg_data_mem_addr               0x00083410

set MSG_FLASH_DATA_BUF_SIZE             [expr {$MSG_BUF_SIZE / 1024 * 1024}]

set rtl872x_ready                       0


proc openocd_read_register {reg} {
    set value ""
    mem2array value 32 $reg 1
    return $value(0)
}

proc openocd_write_register {reg value} {
    mww $reg $value
}

proc openocd_print_memory {address length} {
    echo "Initializing RTL872x and halt it"
    init
    reset halt
    set value ""
    mem2array value 8 $address $length
    echo ""
    echo "---------------------------------------------------------"
    echo "  Offset| 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"
    echo "---------------------------------------------------------"
    for {set i 0} {$i < $length} {incr i} {
        if {$i % 16 == 0} {
            echo -n "[format {%08X} [expr {$address + $i}]]| "
        }

        echo -n "[format %02X $value($i)] "
        if {[expr $i > 0 && ($i+1) % 16 == 0] || [expr $i == $length - 1]} {
            echo ""
        }
    }
    echo "---------------------------------------------------------"
    echo ""
}

proc progress_bar {cur tot} {
    # if you don't want to redraw all the time, uncomment and change ferquency
    #if {$cur % ($tot/300)} { return }
    # set to total width of progress bar
    set total 76
    set half 0 ;#[expr {$total/2}]
    set percent [expr {100.*$cur/$tot}]
    set val "[format "%2.0f%%" $percent]"
    set str "\r  |[string repeat = [
                expr {round($percent*$total/100)}]][
                        string repeat { } [expr {$total-round($percent*$total/100)}]]|"
    set str "[string range $str 0 $half]$val[string range $str [expr {$half+[string length $val]-1}] end]"
    puts -nonewline stderr $str
}

proc rtl872x_init {} {
    global rtl872x_ready
    global pc_msg_avail_mem_addr
    global mcu_msg_avail_mem_addr
    global RTL872x_REG_BOOT_CFG
    global RTL872x_REG_LP_KM4_CTRL
    global rtl872x_flash_loader

    if {[expr {$rtl872x_ready == 0}]} {
        echo "initializing rtl872x"
        init
        reset halt
        array2mem rtl872x_flash_loader 32 0x00082000 [array size rtl872x_flash_loader]

        # Set rom boot BIT to boot to RAM
        set reg_val [openocd_read_register $RTL872x_REG_BOOT_CFG]
        set reg_val [expr $reg_val & 0x0000FFFF]
        set reg_val [expr $reg_val | (0x01 << 26)]
        echo "boot cfg reg write: [format {0x%08X} $reg_val]"
        mww $RTL872x_REG_BOOT_CFG $reg_val

        # Stop KM4 when downloading flash
        set reg_val [openocd_read_register $RTL872x_REG_LP_KM4_CTRL]
        set reg_val [expr $reg_val & ~(1 << 3)]
        set reg_val [expr $reg_val & ~(1 << 24)]
        mww $RTL872x_REG_LP_KM4_CTRL $reg_val

        # Reset communication register
        openocd_write_register $pc_msg_avail_mem_addr 0
        openocd_write_register $mcu_msg_avail_mem_addr 0

        reset run

        set rtl872x_ready 1
    }
}

proc wait_response {timeout} {
    global mcu_msg_avail_mem_addr
    global mcu_msg_data_mem_addr
    set startTime [clock milliseconds]
    set newline 0
    # echo "\n-mcu msg avail: [openocd_read_register [expr $mcu_msg_avail_mem_addr]]"
    while {[expr [openocd_read_register [expr $mcu_msg_avail_mem_addr]] == 0]} {
        if {$timeout && [expr [clock milliseconds] - $startTime] > $timeout} {
            return 0
        }
    }
    # echo "\n+mcu msg avail: [openocd_read_register [expr $mcu_msg_avail_mem_addr]]"
    openocd_write_register $mcu_msg_avail_mem_addr 0
}

proc rtl872x_flash_read_id {} {
    global pc_msg_avail_mem_addr
    global mcu_msg_avail_mem_addr
    global pc_msg_data_mem_addr
    global mcu_msg_data_mem_addr
    global CMD_FLASH_READ_ID

    rtl872x_init
    halt
    mww [expr {$pc_msg_data_mem_addr + 0x00}] $CMD_FLASH_READ_ID
    mww $pc_msg_avail_mem_addr 1
    resume
    wait_response 0
    set id [openocd_read_register [expr {$mcu_msg_data_mem_addr + 0x04}]]
    echo "RTL872x flash id: $id"
}

proc rtl872x_flash_dump {address length} {
    global FLASH_MMU_START_ADDR
    rtl872x_init
    set address [expr $address + $FLASH_MMU_START_ADDR]
    openocd_print_memory $address $length
}

proc rtl872x_flash_erase_all {} {
    global pc_msg_avail_mem_addr
    global mcu_msg_avail_mem_addr
    global pc_msg_data_mem_addr
    global mcu_msg_data_mem_addr
    global CMD_FLASH_ERASE_ALL

    set startTime [clock milliseconds]
    rtl872x_init
    halt
    mww [expr {$pc_msg_data_mem_addr + 0x00}] $CMD_FLASH_ERASE_ALL
    mww $pc_msg_avail_mem_addr 1
    resume
    wait_response 0
    set timelapse [expr [expr [clock milliseconds] - $startTime] / 1000.0]
    echo "\nO.K. Flash mass erase time: [format "%.3f" $timelapse]s"
}

proc rtl872x_flash_write_bin {file address} {
    global RTL872x_REG_BOOT_CFG

    # Enable auto_erase and auto_verify 
    rtl872x_flash_write_bin_ext $file $address 1 1

    # Set rom boot BIT to boot to flash
    set reg_val [openocd_read_register $RTL872x_REG_BOOT_CFG]
    set reg_val [expr $reg_val & 0x0000FFFF]
    mww $RTL872x_REG_BOOT_CFG $reg_val
}

proc rtl872x_flash_write_bin_ext {file address auto_erase auto_verify} {
    global pc_msg_avail_mem_addr
    global mcu_msg_avail_mem_addr
    global pc_msg_data_mem_addr
    global mcu_msg_data_mem_addr
    global CMD_FLASH_WRITE
    global CMD_FLASH_ERASE
    global CMD_FLASH_VERIFY
    global FLASH_SECTOR_SIZE
    global MSG_FLASH_DATA_BUF_SIZE
    global RESULT_OK
    global RESULT_ERROR

    rtl872x_init
    set size [file size $file]

    echo ""
    echo "Downloading file: [file tail $file], Size: $size, Address: $address"

    if {[expr $address % $FLASH_SECTOR_SIZE != 0]} {
        error "address must be sector-aligned"
    }

    # Erase flash
    set startTime [clock milliseconds]
    if {$auto_erase} {
        echo "Erase flash:"
        set sector_start_num [expr $address / $FLASH_SECTOR_SIZE]
        set sector_count [expr [expr $size + $FLASH_SECTOR_SIZE - 1] / $FLASH_SECTOR_SIZE]

        mww [expr {$pc_msg_data_mem_addr + 0x00}] $CMD_FLASH_ERASE
        mww [expr {$pc_msg_data_mem_addr + 0x04}] $address
        mww [expr {$pc_msg_data_mem_addr + 0x08}] $size
        mww $pc_msg_avail_mem_addr 1
    
        set fake_progress 0
        while {[wait_response 500] == 0} {
            sleep 500
            set fake_progress [expr $fake_progress + 2]
            progress_bar $fake_progress 100
        }
        progress_bar 100 100
        echo "\nO.K. Erase time: [format "%.3f" [expr [expr [clock milliseconds] - $startTime] / 1000.0]]s"
    }

    # Write binary file
    # The load_image command has a size limitation for the load file.
    # On Realtek MCU, the RAM address stats at 0x80000, which allows us to load a up-to-512KB file.
    # If the file is larger than 512KB, we'll split it into sevral 512KB pieces.
    set tmp_file_path "/tmp"
    global tcl_platform
    if {$tcl_platform(platform) eq "windows"} {
        set tmp_file_path "C:/tmp"
        file mkdir $tmp_file_path
    }

    set file_counts 0
    set f [open $file rb]
    fconfigure $f -translation binary
    for {set i 0} {$i < $size} {set i [expr $i + [expr 512 * 1024]]} {
        set bin_data [read $f [expr 512 * 1024]]
        set f_tmp [open "$tmp_file_path/op_file_$file_counts.bin" wb]
        puts $f_tmp $bin_data
        close $f_tmp
        incr file_counts
    }
    close $f

    set startTime [clock milliseconds]
    echo "Program flash: $file_counts files in total"
    for {set file_num 0} {$file_num < $file_counts} {incr file_num} {
        set op_file "$tmp_file_path/op_file_$file_num.bin"
        set op_file_size [expr [file size $op_file] - 1] ;# file read command will read an extra newline char
        set op_file_address [expr $address + [expr 512 * 1024 * $file_num]]
        # echo "file: $tmp_file_path/op_file_$file_num.bin, size: $op_file_size, address: [format "%X" $op_file_address]"
        for {set i 0} {$i < $op_file_size} {set i [expr $i + $MSG_FLASH_DATA_BUF_SIZE]} {
            set write_size [expr [expr $op_file_size - $i > $MSG_FLASH_DATA_BUF_SIZE] ? $MSG_FLASH_DATA_BUF_SIZE : [expr $op_file_size - $i]]
            mww [expr {$pc_msg_data_mem_addr + 0x00}] $CMD_FLASH_WRITE
            mww [expr {$pc_msg_data_mem_addr + 0x04}] [expr $op_file_address + $i]
            mww [expr {$pc_msg_data_mem_addr + 0x08}] $write_size
            load_image $op_file [expr $pc_msg_data_mem_addr + 0x10 - $i] bin [expr $pc_msg_data_mem_addr + 0x10] $write_size 
            # echo "write size: $write_size, offset: $i, address: [expr $op_file_address + $i]"
            progress_bar $i $op_file_size
            mww $pc_msg_avail_mem_addr 1
            wait_response 0
        }
    }
    progress_bar 100 100
    set timelapse [expr [expr [clock milliseconds] - $startTime] / 1000.0]
    echo "\nO.K. Program time: [format "%.3f" $timelapse]s, speed: [format "%.3f" [expr [expr $size / 1024.0] / $timelapse]]KB/s"
 
    if {$auto_verify} {
        # Verify binary file
        set startTime [clock milliseconds]
        echo "Verify flash: "
        set crc32 [crc32_binary $file]
        mww [expr {$pc_msg_data_mem_addr + 0x00}] $CMD_FLASH_VERIFY
        mww [expr {$pc_msg_data_mem_addr + 0x04}] $address
        mww [expr {$pc_msg_data_mem_addr + 0x08}] $size
        mww [expr {$pc_msg_data_mem_addr + 0x0C}] $crc32
        mww $pc_msg_avail_mem_addr 1
        wait_response 0
        set timelapse [expr [expr [clock milliseconds] - $startTime] / 1000.0]
        if {[expr [openocd_read_register [expr {$mcu_msg_data_mem_addr + 0x00}]] == $RESULT_OK]} {
            echo "O.K. Verify time: [format "%.3f" [expr [expr [clock milliseconds] - $startTime] / 1000.0]]s"
        } else {
            error "Flash Verify FAILED!"
        }
    }
    echo "** Programming Finished **"
}

proc rtl872x_verify_image {file address} {
    rtl872x_init
}

proc rtl872x_flash_read {} {
    rtl872x_init
}

proc rtl872x_flash_erase_sector {} {
    rtl872x_init
}

proc rtl872x_read_efuse_mac {} {
    global pc_msg_avail_mem_addr
    global mcu_msg_avail_mem_addr
    global pc_msg_data_mem_addr
    global mcu_msg_data_mem_addr
    global CMD_READ_EFUSE_MAC
    global WIFI_MAC_SIZE
    global RESULT_OK

    rtl872x_init
    halt
    mww [expr {$pc_msg_data_mem_addr + 0x00}] $CMD_READ_EFUSE_MAC
    mww $pc_msg_avail_mem_addr 1
    resume
    wait_response 0
    set mac ""
    mem2array mac 8 [expr {$mcu_msg_data_mem_addr + 0x04}] $WIFI_MAC_SIZE
    if {[expr [openocd_read_register [expr {$mcu_msg_data_mem_addr + 0x00}]] == $RESULT_OK]} {
        set macstr [format "%02X:%02X:%02X:%02X:%02X:%02X" $mac(0) $mac(1) $mac(2) $mac(3) $mac(4) $mac(5)]
        echo "MAC: $macstr"
    } else {
        error "Failed to read WIFI MAC"
    }
}

proc rtl872x_wdg_reset {} {
    global RTL872x_REG_BOOT_CFG
    set WDG_BASE 				0x40002800
    set WDG_BIT_ENABLE			[expr (0x00000001 << 16)]
    set WDG_BIT_CLEAR			[expr (0x00000001 << 24)]
    set WDG_BIT_RST_MODE		[expr (0x00000001 << 30)]
    set WDG_BIT_ISR_CLEAR		[expr (0x00000001 << 31)]
    set WDT_DIV_FRAC            1666
    set wdg_reg $WDT_DIV_FRAC
    set wdg_reg [expr $wdg_reg & ~(0x00FF0000)]
    set wdg_reg [expr $wdg_reg | ($WDG_BIT_CLEAR | $WDG_BIT_RST_MODE | $WDG_BIT_ISR_CLEAR | $WDG_BIT_ENABLE)]

    set reg_val [openocd_read_register $RTL872x_REG_BOOT_CFG]
    set reg_val [expr $reg_val & 0x0000FFFF]
    set reg_val [expr $reg_val & ~(0x01 << 26)]
    echo "boot cfg reg write: [format {0x%08X} $reg_val]"
    mww $RTL872x_REG_BOOT_CFG $reg_val

    mww $WDG_BASE $wdg_reg
}

#
# CRC32 implementation
#
# Reference: https://wiki.tcl-lang.org/page/CRC
#
# CRCTABLE is the pre-calculated lookup table provided for this
# implementation:
#
set CRCTABLE {      0x00000000 0x77073096 0xEE0E612C 0x990951BA
                    0x076DC419 0x706AF48F 0xE963A535 0x9E6495A3
                    0x0EDB8832 0x79DCB8A4 0xE0D5E91E 0x97D2D988
                    0x09B64C2B 0x7EB17CBD 0xE7B82D07 0x90BF1D91
                    0x1DB71064 0x6AB020F2 0xF3B97148 0x84BE41DE
                    0x1ADAD47D 0x6DDDE4EB 0xF4D4B551 0x83D385C7
                    0x136C9856 0x646BA8C0 0xFD62F97A 0x8A65C9EC
                    0x14015C4F 0x63066CD9 0xFA0F3D63 0x8D080DF5
                    0x3B6E20C8 0x4C69105E 0xD56041E4 0xA2677172
                    0x3C03E4D1 0x4B04D447 0xD20D85FD 0xA50AB56B
                    0x35B5A8FA 0x42B2986C 0xDBBBC9D6 0xACBCF940
                    0x32D86CE3 0x45DF5C75 0xDCD60DCF 0xABD13D59
                    0x26D930AC 0x51DE003A 0xC8D75180 0xBFD06116
                    0x21B4F4B5 0x56B3C423 0xCFBA9599 0xB8BDA50F
                    0x2802B89E 0x5F058808 0xC60CD9B2 0xB10BE924
                    0x2F6F7C87 0x58684C11 0xC1611DAB 0xB6662D3D
                    0x76DC4190 0x01DB7106 0x98D220BC 0xEFD5102A
                    0x71B18589 0x06B6B51F 0x9FBFE4A5 0xE8B8D433
                    0x7807C9A2 0x0F00F934 0x9609A88E 0xE10E9818
                    0x7F6A0DBB 0x086D3D2D 0x91646C97 0xE6635C01
                    0x6B6B51F4 0x1C6C6162 0x856530D8 0xF262004E
                    0x6C0695ED 0x1B01A57B 0x8208F4C1 0xF50FC457
                    0x65B0D9C6 0x12B7E950 0x8BBEB8EA 0xFCB9887C
                    0x62DD1DDF 0x15DA2D49 0x8CD37CF3 0xFBD44C65
                    0x4DB26158 0x3AB551CE 0xA3BC0074 0xD4BB30E2
                    0x4ADFA541 0x3DD895D7 0xA4D1C46D 0xD3D6F4FB
                    0x4369E96A 0x346ED9FC 0xAD678846 0xDA60B8D0
                    0x44042D73 0x33031DE5 0xAA0A4C5F 0xDD0D7CC9
                    0x5005713C 0x270241AA 0xBE0B1010 0xC90C2086
                    0x5768B525 0x206F85B3 0xB966D409 0xCE61E49F
                    0x5EDEF90E 0x29D9C998 0xB0D09822 0xC7D7A8B4
                    0x59B33D17 0x2EB40D81 0xB7BD5C3B 0xC0BA6CAD
                    0xEDB88320 0x9ABFB3B6 0x03B6E20C 0x74B1D29A
                    0xEAD54739 0x9DD277AF 0x04DB2615 0x73DC1683
                    0xE3630B12 0x94643B84 0x0D6D6A3E 0x7A6A5AA8
                    0xE40ECF0B 0x9309FF9D 0x0A00AE27 0x7D079EB1
                    0xF00F9344 0x8708A3D2 0x1E01F268 0x6906C2FE
                    0xF762575D 0x806567CB 0x196C3671 0x6E6B06E7
                    0xFED41B76 0x89D32BE0 0x10DA7A5A 0x67DD4ACC
                    0xF9B9DF6F 0x8EBEEFF9 0x17B7BE43 0x60B08ED5
                    0xD6D6A3E8 0xA1D1937E 0x38D8C2C4 0x4FDFF252
                    0xD1BB67F1 0xA6BC5767 0x3FB506DD 0x48B2364B
                    0xD80D2BDA 0xAF0A1B4C 0x36034AF6 0x41047A60
                    0xDF60EFC3 0xA867DF55 0x316E8EEF 0x4669BE79
                    0xCB61B38C 0xBC66831A 0x256FD2A0 0x5268E236
                    0xCC0C7795 0xBB0B4703 0x220216B9 0x5505262F
                    0xC5BA3BBE 0xB2BD0B28 0x2BB45A92 0x5CB36A04
                    0xC2D7FFA7 0xB5D0CF31 0x2CD99E8B 0x5BDEAE1D
                    0x9B64C2B0 0xEC63F226 0x756AA39C 0x026D930A
                    0x9C0906A9 0xEB0E363F 0x72076785 0x05005713
                    0x95BF4A82 0xE2B87A14 0x7BB12BAE 0x0CB61B38
                    0x92D28E9B 0xE5D5BE0D 0x7CDCEFB7 0x0BDBDF21
                    0x86D3D2D4 0xF1D4E242 0x68DDB3F8 0x1FDA836E
                    0x81BE16CD 0xF6B9265B 0x6FB077E1 0x18B74777
                    0x88085AE6 0xFF0F6A70 0x66063BCA 0x11010B5C
                    0x8F659EFF 0xF862AE69 0x616BFFD3 0x166CCF45
                    0xA00AE278 0xD70DD2EE 0x4E048354 0x3903B3C2
                    0xA7672661 0xD06016F7 0x4969474D 0x3E6E77DB
                    0xAED16A4A 0xD9D65ADC 0x40DF0B66 0x37D83BF0
                    0xA9BCAE53 0xDEBB9EC5 0x47B2CF7F 0x30B5FFE9
                    0xBDBDF21C 0xCABAC28A 0x53B39330 0x24B4A3A6
                    0xBAD03605 0xCDD70693 0x54DE5729 0x23D967BF
                    0xB3667A2E 0xC4614AB8 0x5D681B02 0x2A6F2B94
                    0xB40BBE37 0xC30C8EA1 0x5A05DF1B 0x2D02EF8D
}

proc crc32 {instr} {
    global CRCTABLE

    set crc_value 0xFFFFFFFF
    foreach c [split $instr {}] {
        set crc_value [expr {[lindex $CRCTABLE [expr {($crc_value ^ [scan $c %c])&0xff}]]^(($crc_value>>8)&0xffffff)}]
    }
    return [expr {$crc_value ^ 0xFFFFFFFF}]
}

proc crc32_binary {file} {
    set f [open $file rb]
    fconfigure $f -translation binary
    set data [read $f]
    close $f
    return [crc32 $data]
}
