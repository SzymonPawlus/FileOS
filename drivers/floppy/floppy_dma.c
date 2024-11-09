//
// Created by szymon on 04/08/2021.
//

#include "floppy_dma.h"
#include "../../cpu/isr.h"
#include "../../kernel/util.h"
#include "../../fs/vfs.h"
#include "../../libc/math.h"

static int floppy_dma_init(enum FloppyTransmitDirection dir, uint32_t size);
static enum FloppyErrors floppy_do_track(const FLOPPY_DEVICE *drive, DMA dma, enum FloppyTransmitDirection dir, uint32_t size, uint32_t *seg_read);
static int floppy_calibrate(const FLOPPY_DEVICE *drive);
static int floppy_seek(uint8_t cyli, uint32_t head, const FLOPPY_DEVICE *drive);
static int floppy_reset(const FLOPPY_DEVICE *drive);

// Transfer buffer
#define floppy_dmalen 0x8000
extern unsigned int _DMA_BUFFER_POS;
uint8_t* floppy_dmabuf = (uint8_t *) &_DMA_BUFFER_POS;

// TODO - Add timeouts
volatile int floppy_dma_waiting = 1;
void irq6_handler(registers_t regs){
    floppy_dma_waiting = 0;
}

static void floppy_wait(){
    while(floppy_dma_waiting);
    floppy_dma_waiting = 1;
}


// Public functions
int read_floppy_dma(FLOPPY_DEVICE *drive, uint8_t* buffer, uint32_t sectors, uint32_t lba){
    uint32_t seg_read, total_sectors = 0;

    while(total_sectors < sectors){
        DMA dma = lba_2_chs(lba + total_sectors, drive);
        uint32_t size = min(512 * (sectors - total_sectors), 512 * 8); // TODO - maybe solve in better way
        // Gives page fault when there's more than 4KB of mem
        int error = floppy_do_track(drive, dma, floppy_dir_read, size, &seg_read);
        if(error & E_ERROR) return error;
        k_memcpy(floppy_dmabuf, (char*)(buffer + total_sectors * 512), (int)(512 * seg_read));
        total_sectors += seg_read;
    }
    return 0;

}

int write_floppy_dma(FLOPPY_DEVICE *drive, uint8_t* buffer, uint32_t sectors, uint32_t lba){
    uint32_t seg_read, total_sectors = 0;
    while(total_sectors < sectors){
        uint32_t size = min(512 * (sectors - total_sectors), 512 * 8);
        DMA dma = lba_2_chs(lba + total_sectors, drive);
        k_memcpy((char*)buffer, floppy_dmabuf, (int)size);
        int error = floppy_do_track(drive, dma, floppy_dir_write, size, &seg_read);
        if(error & E_ERROR) return error;
        total_sectors += seg_read;
    }
    return 0;
}

void init_floppy_dma() {
    register_interrupt_handler(IRQ6, irq6_handler);
    set_DOR(0x0c);
}

// Private functions
static int floppy_dma_init(enum FloppyTransmitDirection dir, uint32_t size){
    union {
        uint8_t b[4];
        uint32_t l;
    } a, c;

    a.l = (uint32_t) floppy_dmabuf;
    c.l = (uint32_t) size - 1;

    if(size > floppy_dmalen) {
        return E_FLOPPY_DMA_BUFFER_BIG | ED_FLOPPY | E_ERROR;
    }

    if((a.l >> 24) || (c.l >> 16) || (((a.l&0xffff)+c.l)>>16)){
        return E_FLOPPY_DMA_BUFFER_INVALID_LOC | ED_FLOPPY | E_ERROR;
    }

    uint8_t mode;
    switch (dir) {
        case floppy_dir_read: {
            mode = 0x46;
            break;
        }
        case floppy_dir_write: {
            mode = 0x4a;
            break;
        }
        default:
        {
            return E_FLOPPY_INVALID_DATA_DIRECTION | ED_FLOPPY | E_ERROR;
        }
    }

    port_byte_out(0x0a, 0x06);
    port_byte_out(0x0c, 0xff);
    port_byte_out(0x04, a.b[0]);
    port_byte_out(0x04, a.b[1]);

    port_byte_out(0x81, a.b[2]);

    port_byte_out(0x0c, 0xff);
    port_byte_out(0x05, c.b[0]);
    port_byte_out(0x05, c.b[1]);

    port_byte_out(0x0b, mode);
    port_byte_out(0x0a, 0x02);

    return E_FLOPPY_NO_ERROR;
}

static enum FloppyErrors floppy_do_track(const FLOPPY_DEVICE *drive, DMA dma, enum FloppyTransmitDirection dir, uint32_t size, uint32_t *seg_read) {
    if(drive->type == F_NO_DRIVE) return E_FLOPPY_NO_DRIVE | ED_FLOPPY | E_ERROR;
    select_drive(drive->drive_number);
    uint8_t cmd;

    switch(dir) {
        case floppy_dir_read:
            cmd = READ_DATA | MT | MF;
            break;
        case floppy_dir_write:
            cmd = WRITE_DATA | MT | MF;
            break;
        default:
            return E_FLOPPY_INVALID_DATA_DIRECTION; // not reached, but pleases "cmd used uninitialized"
    }

    // TODO - Extract calculating sectors
    uint32_t sectors = (size / 512) + ((size % 512) != 0);
    uint32_t final_preview = sectors + dma.sector - 1;
    uint32_t final = final_preview > drive->sectors_per_track ? drive->sectors_per_track : final_preview;
    *seg_read = final - dma.sector + 1;


    // seek both heads
    if(floppy_seek(dma.cylinder, 0, drive)) return -1;
    if(floppy_seek(dma.cylinder, 1, drive)) return -1;

    int error = E_FLOPPY_NO_ERROR;

    for(int i = 0; i < 20; i++) {
        floppy_motor(1, drive->drive_number);

        // init dma..
        // TODO - forward errors
        floppy_dma_init(dir, size);

        floppy_write_cmd(cmd);  // set above for current direction
        floppy_write_cmd(dma.head << 2);    // 0:0:0:0:0:HD:US1:US0 = head and drive
        floppy_write_cmd(dma.cylinder);   // cylinder
        floppy_write_cmd(dma.head);  // first head (should match with above)
        floppy_write_cmd(dma.sector);// first info, strangely counts from 1
        floppy_write_cmd(2);    // bytes/info, 128*2^x (x=2 -> 512)
        floppy_write_cmd(final);   // number of tracks to operate on
        floppy_write_cmd(0x1b); // GAP3 length, 27 is default for 3.5"
        floppy_write_cmd(0xff); // data length (0xff if B/S != 0)

        // TODO - Add timeout to wait function
        floppy_wait(); // don't SENSE_INTERRUPT here!

        // first read status information
        unsigned char st0, st1, st2, rcy, rhe, rse, bps;
        st0 = floppy_read_data();
        st1 = floppy_read_data();
        st2 = floppy_read_data();


        rcy = floppy_read_data();
        rhe = floppy_read_data();
        rse = floppy_read_data();
        // bytes per info, should be what we programmed in
        bps = floppy_read_data();


        error = calculate_error(&st0, &st1, &st2, &bps);

        if(!error){
            floppy_motor(0, drive->drive_number);
            return E_FLOPPY_NO_ERROR;
        }
        if(error == E_FLOPPY_NOT_WRITABLE) return error | ED_FLOPPY | E_ERROR;
    }

    return error | ED_FLOPPY | E_ERROR;
}

static int floppy_reset(const FLOPPY_DEVICE *drive) {
    port_byte_out(DIGITAL_OUTPUT_REGISTER, 0x00);
    port_byte_out(DIGITAL_OUTPUT_REGISTER, 0x0c);

    floppy_wait();

    {
        uint32_t st0, cyl;
        floppy_sense_interrupt(&st0, &cyl);
    }

    port_byte_out(CONFIGURATION_CONTROL_REGISTER, 0x00);

    floppy_write_cmd(SPECIFY);
    floppy_write_cmd(0xdf);
    floppy_write_cmd(0x02);


    if(floppy_calibrate(drive)) return -1;
    return 0;
}

static int floppy_calibrate(const FLOPPY_DEVICE *drive) {
    uint32_t st0, cyl = -1;

    floppy_motor(1, drive->drive_number);

    for (int i = 0; i < 10; ++i) {
        floppy_write_cmd(RECALIBRATE);
        floppy_write_cmd(0);

        floppy_wait();

        floppy_sense_interrupt(&st0, &cyl);

        if(st0 & 0xc0){
            kprint("Floppy calibrate: st0");
            continue;
        }

        if(!cyl){
            floppy_motor(0, drive->drive_number);
            return 0;
        }
    }

    kprint("Floppy calibrate: 10 retries exhausted");
    floppy_motor(0, drive->drive_number);
    return -1;
}

static int floppy_seek(uint8_t cyli, uint32_t head, const FLOPPY_DEVICE *drive) {
    uint32_t st0, cyl;

    floppy_motor(1, drive->drive_number);

    for (int i = 0; i < 10; ++i) {
        floppy_write_cmd(SEEK);
        floppy_write_cmd(head << 2);
        floppy_write_cmd(cyli);

        floppy_wait();

        floppy_sense_interrupt(&st0, &cyl);

        if(st0 & 0xc0)
            continue;

        if(cyl == cyli){
            floppy_motor(0, drive->drive_number);
            return E_FLOPPY_NO_ERROR;
        }
    }

    floppy_motor(0, drive->drive_number);
    return E_FLOPPY_EXHAUSTED | ED_FLOPPY | E_ERROR;
}
