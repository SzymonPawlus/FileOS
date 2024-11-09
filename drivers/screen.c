#include "screen.h"
#include "ports.h"
#include "../kernel/util.h"

// Private functions declarations
int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, char attr);
int get_offset(int col, int row);
int get_offset_row(int offset);
int get_offset_column(int offset);


// Public functions
void
clear_screen(){
    int screen_size = MAX_COLS * MAX_ROWS;
    char* screen = (char *) VIDEO_ADDRESS;

    for(int i = 0; i < screen_size; ++i){
        screen[2 * i] = ' ';
        screen[2 * i + 1] = WHITE_ON_BLACK;
    }
}

void
kprint_at_size(char* message, uint32_t size, int col, int row) {
    int offset;

    if(col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else{
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_column(offset);
    }

    for (int i = 0; i < size; ++i){
        offset = print_char(message[i], col, row, WHITE_ON_BLACK);
        row = get_offset_row(offset);
        col = get_offset_column(offset);
    }

}

void
kprint_size(char* message, uint32_t size){
    kprint_at_size(message, size, -1, -1);
}

void
kprint_at(char *message, int col, int row){
    int offset;

    if(col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else{
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_column(offset);
    }

    int i = 0;
    while(message[i] != 0){
        offset = print_char(message[i++], col, row, WHITE_ON_BLACK);
        row = get_offset_row(offset);
        col = get_offset_column(offset);
    }

}

void
kprint_char_at(char letter, int col, int row){
    int offset;

    if(col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else{
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_column(offset);
    }

    int i = 0;
    offset = print_char(letter, col, row, WHITE_ON_BLACK);
    row = get_offset_row(offset);
    col = get_offset_column(offset);
}

void
kprint_char(char letter){
    kprint_char_at(letter, -1, -1);
}

void
kprint(char* message) {
    kprint_at(message, -1, -1);
}


// Private funciton implementations
int
get_cursor_offset(){
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8;
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2;
}

void
set_cursor_offset(int offset){
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

int
print_char(char c, int col, int row, char attr){
    unsigned char* vid = (unsigned char*) VIDEO_ADDRESS;
    if(!attr) attr = WHITE_ON_BLACK;

    // ERROR - Not on screen
    if(col >= MAX_COLS || row >= MAX_ROWS){
        vid[2 * (MAX_COLS) * (MAX_ROWS) - 2] = 'E';
        vid[2 * (MAX_COLS) * (MAX_ROWS) - 1] = RED_ON_BLACK;
        return get_offset(col, row);
    }

    int offset = (col < 0 || row < 0) ? get_cursor_offset() : get_offset(col, row);

    if(c == '\n'){
        row = get_offset_row(offset);
        offset = get_offset(0, row + 1);
    }else if(c == '\b') {
        offset -= 2;
        vid[offset]     = '\0';
        vid[offset + 1] = WHITE_ON_BLACK;

    }else{
        vid[offset] = c;
        vid[offset + 1] = attr;
        offset += 2;
    }


    if(offset >= MAX_ROWS * MAX_COLS * 2){
        for (int i = 1; i < MAX_ROWS; ++i)
            k_memcpy((const char *) (get_offset(0, i) + VIDEO_ADDRESS), (char *) (get_offset(0, i - 1) + VIDEO_ADDRESS), MAX_COLS * 2);

        char* last_line = (char *) (get_offset(0, MAX_ROWS - 1) + VIDEO_ADDRESS);
        for (int i = 0; i < MAX_COLS * 2; ++i) last_line[i] = 0;

        offset -= 2 * MAX_COLS;
    }

    set_cursor_offset(offset);
    return offset;
}

int
get_offset(int col, int row){
    return 2 * (col + MAX_COLS * row);
}

int get_offset_row(int offset){
    return offset / (2 * MAX_COLS);
}

int get_offset_column(int offset){
    return (offset - (get_offset_row(offset) * 2 * MAX_COLS))/2;
}



