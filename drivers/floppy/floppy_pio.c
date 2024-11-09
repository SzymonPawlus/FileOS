//
// Created by szymon on 04/08/2021.
//

#include "floppy_pio.h"
#include "../../kernel/util.h"
#include "../../fs/vfs.h"
#include "../../libc/math.h"

int floppy_transfer_pio(
        const FLOPPY_DEVICE *drive,
        DMA dma,
        enum FloppyTransmitDirection dir,
        uint32_t size,
        uint32_t *seg_read,
        void (*callback)(void));

static uint32_t floppy_buffer_index = 0;
static uint32_t floppy_buffer_size  = 0;
static uint8_t* floppy_buffer       = 0;

void read_callback(){
    if(floppy_buffer_index >= floppy_buffer_size) port_byte_in(DATA_FIFO);
    else floppy_buffer[floppy_buffer_index++] = port_byte_in(DATA_FIFO);
}

void write_callback() {
    if(floppy_buffer_index >= floppy_buffer_size) port_byte_out(DATA_FIFO, 0x00);
    else port_byte_out(DATA_FIFO, floppy_buffer[floppy_buffer_index++]);
}


void init_floppy_pio(){
    set_DOR(0b00000100);
}

// Check for correctness
int read_floppy_pio(FLOPPY_DEVICE* drive, uint8_t* buffer, uint32_t sectors, uint32_t lba){
    uint32_t seg_read, total_sectors = 0;
    floppy_buffer = buffer;

    while(total_sectors < sectors){
        DMA dma = lba_2_chs(lba + total_sectors, drive);
        uint32_t size = 512 * (sectors - total_sectors);
        int error = floppy_transfer_pio(drive, dma, floppy_dir_read, size, &seg_read, read_callback);
        if(error & E_ERROR) return error;
        total_sectors += seg_read;
    }
    return 0;
}

int write_floppy_pio(FLOPPY_DEVICE* drive, uint8_t* buffer, uint32_t sectors, uint32_t lba){
    uint32_t seg_read, total_sectors = 0;
    floppy_buffer = buffer;

    while(total_sectors < sectors){
        uint32_t size = min(512 * (sectors - total_sectors), 512 * drive->sectors_per_track);
        DMA dma = lba_2_chs(lba + total_sectors, drive);
        int error = floppy_transfer_pio(drive, dma, floppy_dir_write, size, &seg_read, write_callback);
        if(error & E_ERROR) return error;
        total_sectors += seg_read;
    }
    return 0;
}

int floppy_transfer_pio(
        const FLOPPY_DEVICE *drive,
        DMA dma,
        enum FloppyTransmitDirection dir,
        uint32_t size,
        uint32_t *seg_read,
        void (*callback)(void))
{
    if(drive->type == F_NO_DRIVE) return E_FLOPPY_NO_DRIVE;

    select_drive(drive->drive_number);
    // Write appropriate command

    uint32_t sectors = (size / 512) + ((size % 512) != 0);
    uint32_t final_preview = sectors + dma.sector - 1;
    uint32_t final = final_preview > drive->sectors_per_track ? drive->sectors_per_track : final_preview;
    *seg_read = final - dma.sector + 1;
    floppy_buffer_size = 512 * sectors;

    int error = E_FLOPPY_NO_ERROR;

    for (int i = 0; i < 3; ++i) {
        floppy_motor(1, drive->drive_number);

        if(dir == floppy_dir_write) floppy_write_cmd(MT | MF | WRITE_DATA);
        if(dir == floppy_dir_read) floppy_write_cmd(MT | MF | READ_DATA);
        floppy_write_cmd(dma.head << 2);    // 0:0:0:0:0:HD:US1:US0 = head and drive
        floppy_write_cmd(dma.cylinder);   // cylinder
        floppy_write_cmd(dma.head);  // first head (should match with above)
        floppy_write_cmd(dma.sector);// first info, strangely counts from 1
        floppy_write_cmd(2);    // bytes/info, 128*2^x (x=2 -> 512)
        floppy_write_cmd(final);   // number of tracks to operate on
        floppy_write_cmd(0x1b); // GAP3 length, 27 is default for 3.5"
        floppy_write_cmd(0xff); // data length (0xff if B/S != 0)

        while((get_MSR() & 0x20) == 0x20){
            // TODO - Add timeouts to conditions
            while((get_MSR() & 0x80) != 0x80);
            while((get_MSR() & (RQM | NDMA)) == (RQM | NDMA))
                callback();
        }

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

    return error | E_ERROR | ED_FLOPPY;
}