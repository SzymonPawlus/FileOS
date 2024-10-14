//
// Created by szymon on 31/07/2021.
//

#include "keyboard.h"
#include "ports.h"
#include "screen.h"

char key;

char* print_letter(u8 scancode);

isr_t kernel_callback;

int high = 0;

static void keyboard_callback(registers_t reg) {
    u8 scancode = port_byte_in(0x60);
    char* letter = print_letter(scancode);
    if(letter[0] == 'S') high = !high;
    else if(letter[0]) {
        if('a' <= letter[0] && letter[0] <= 'z' )key = letter[0] - high * 32;
        else if(letter[0] == ';') key = high ? ':' : ';';
        else key = letter[0];
        kernel_callback(reg);
    }
}

void init_keyboard(isr_t callback) {
    kernel_callback = callback;
    register_interrupt_handler(IRQ1, keyboard_callback);
}

int read_key_buffer(char* buffer){
    if(!key) return 0;
    *buffer = key;
    return 1;
}

char* print_letter(u8 scancode) {
    switch (scancode) {
        case 0x0:
            return "E";
            break;
        case 0x1:
            return "E";
            break;
        case 0x2:
            return"1";
            break;
        case 0x3:
            return"2";
            break;
        case 0x4:
            return"3";
            break;
        case 0x5:
            return"4";
            break;
        case 0x6:
            return"5";
            break;
        case 0x7:
            return"6";
            break;
        case 0x8:
            return"7";
            break;
        case 0x9:
            return"8";
            break;
        case 0x0A:
            return"9";
            break;
        case 0x0B:
            return"0";
            break;
        case 0x0C:
            return"-";
            break;
        case 0x0D:
            return"+";
            break;
        case 0x0E:
            return"\b";
            break;
        case 0x0F:
            return" ";
            break;
        case 0x10:
            return"Q";
            break;
        case 0x11:
            return"w";
            break;
        case 0x12:
            return"e";
            break;
        case 0x13:
            return"r";
            break;
        case 0x14:
            return"t";
            break;
        case 0x15:
            return"y";
            break;
        case 0x16:
            return"u";
            break;
        case 0x17:
            return"i";
            break;
        case 0x18:
            return"o";
            break;
        case 0x19:
            return"p";
            break;
        case 0x1A:
            return"[";
            break;
        case 0x1B:
            return"]";
            break;
        case 0x1C:
            return"\n";
            break;
        case 0x1D:
            return"S";
            break;
        case 0x1E:
            return"a";
            break;
        case 0x1F:
            return"s";
            break;
        case 0x20:
            return"d";
            break;
        case 0x21:
            return"f";
            break;
        case 0x22:
            return"g";
            break;
        case 0x23:
            return"h";
            break;
        case 0x24:
            return"j";
            break;
        case 0x25:
            return"k";
            break;
        case 0x26:
            return"l";
            break;
        case 0x27:
            return";";
            break;
        case 0x28:
            return"";
            break;
        case 0x29:
            return"`";
            break;
        case 0x2A:
            return "S";
            break;
        case 0x2B:
            return"\\";
            break;
        case 0x2C:
            return"z";
            break;
        case 0x2D:
            return"x";
            break;
        case 0x2E:
            return"c";
            break;
        case 0x2F:
            return"v";
            break;
        case 0x30:
            return"b";
            break;
        case 0x31:
            return"n";
            break;
        case 0x32:
            return"m";
            break;
        case 0x33:
            return",";
            break;
        case 0x34:
            return".";
            break;
        case 0x35:
            return"/";
            break;
        case 0x36:
            return "S";
            // return"Rshift";
            break;
        case 0x37:
            return "";
            // return"Keypad *";
            break;
        case 0x38:
            return "";
            // return"LAlt";
            break;
        case 0x39:
            return" ";
            break;
        default:
            if(scancode == 0x2A + 0x80 || scancode == 0x36 + 0x80)
                return print_letter(scancode - 0x80);
            else return "";
            /* 'keuyp' event corresponds to the 'keydown' + 0x80
             * it may still be a scancode we haven't implemented yet, or
             * maybe a control/escape sequence */
            if (scancode <= 0x7f) {
                // kprint("Unknown key down");
            } else if (scancode <= 0x39 + 0x80) {
                //kprint("key up ");

            } else; // kprint("Unknown key up");;
            break;
    }
}