#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Estrutura para substituir a classe Print
typedef struct {
    size_t (*write_char)(char);
    size_t (*write_buffer)(const char*, size_t);
} Print;

// Protótipos das funções públicas
size_t print_str(Print *p, const char *str);
size_t print_char(Print *p, char c);
size_t print_uchar(Print *p, unsigned char b, int base);
size_t print_int(Print *p, int n, int base);
size_t print_uint(Print *p, unsigned int n, int base);
size_t print_long(Print *p, long n, int base);
size_t print_ulong(Print *p, unsigned long n, int base);
size_t print_float(Print *p, double num, int digits);
size_t println(Print *p);

// Funções auxiliares
static size_t print_number(Print *p, unsigned long n, uint8_t base);
static size_t print_ull_number(Print *p, unsigned long long n64, uint8_t base);
static size_t print_float_helper(Print *p, double number, int digits);

// Implementações

size_t print_write(Print *p, const char *buffer, size_t size) {
    if(p == NULL || p->write_buffer == NULL) return 0;
    return p->write_buffer(buffer, size);
}

size_t print_str(Print *p, const char *str) {
    return print_write(p, str, strlen(str));
}

size_t print_char(Print *p, char c) {
    if(p == NULL || p->write_char == NULL) return 0;
    return p->write_char(c);
}

size_t print_uchar(Print *p, unsigned char b, int base) {
    return print_ulong(p, (unsigned long)b, base);
}

size_t print_int(Print *p, int n, int base) {
    return print_long(p, (long)n, base);
}

size_t print_uint(Print *p, unsigned int n, int base) {
    return print_ulong(p, (unsigned long)n, base);
}

size_t print_long(Print *p, long n, int base) {
    if(base == 0) {
        if(p == NULL) return 0;
        return p->write_buffer((const char*)&n, sizeof(n));
    }
    
    if(base == 10) {
        if(n < 0) {
            size_t t = print_char(p, '-');
            return print_number(p, -n, 10) + t;
        }
        return print_number(p, n, 10);
    }
    return print_number(p, n, base);
}

size_t print_ulong(Print *p, unsigned long n, int base) {
    if(base == 0) {
        if(p == NULL) return 0;
        return p->write_buffer((const char*)&n, sizeof(n));
    }
    return print_number(p, n, base);
}

size_t println(Print *p) {
    return print_write(p, "\r\n", 2);
}

// Funções auxiliares

static size_t print_number(Print *p, unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1];
    char *str = &buf[sizeof(buf)-1];
    *str = '\0';

    if(base < 2) base = 10;

    do {
        char c = n % base;
        n /= base;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while(n);

    return print_write(p, str, strlen(str));
}

static size_t print_ull_number(Print *p, unsigned long long n64, uint8_t base) {
    char buf[64];
    uint8_t i = 0;

    if(base < 2) base = 10;

    uint16_t top = 0xFFFF / base;
    uint16_t th16 = 1;
    uint8_t innerLoops = 0;

    while(th16 < top) {
        th16 *= base;
        innerLoops++;
    }

    while(n64 > th16) {
        uint64_t q = n64 / th16;
        uint16_t r = n64 - q * th16;
        n64 = q;

        for(uint8_t j = 0; j < innerLoops; j++) {
            uint16_t qq = r / base;
            buf[i++] = r - qq * base;
            r = qq;
        }
    }

    uint16_t n16 = n64;
    while(n16 > 0) {
        uint16_t qq = n16 / base;
        buf[i++] = n16 - qq * base;
        n16 = qq;
    }

    size_t bytes = i;
    for(; i > 0; i--) {
        char c = buf[i-1] < 10 ? 
            '0' + buf[i-1] : 
            'A' + buf[i-1] - 10;
        print_char(p, c);
    }
    return bytes;
}

static size_t print_float_helper(Print *p, double number, int digits) {
    if(digits < 0) digits = 2;
    size_t n = 0;

    if(isnan(number)) return print_str(p, "nan");
    if(isinf(number)) return print_str(p, "inf");
    if(number > 4294967040.0) return print_str(p, "ovf");
    if(number < -4294967040.0) return print_str(p, "ovf");

    if(number < 0.0) {
        n += print_char(p, '-');
        number = -number;
    }

    double rounding = 0.5;
    for(uint8_t i = 0; i < digits; ++i)
        rounding /= 10.0;
    
    number += rounding;

    unsigned long int_part = (unsigned long)number;
    double remainder = number - int_part;
    n += print_ulong(p, int_part, 10);

    if(digits > 0) {
        n += print_char(p, '.');
        while(digits-- > 0) {
            remainder *= 10.0;
            unsigned int toPrint = (unsigned int)remainder;
            n += print_uint(p, toPrint, 10);
            remainder -= toPrint;
        }
    }

    return n;
}

size_t print_float(Print *p, double num, int digits) {
    return print_float_helper(p, num, digits);
}

size_t println_float(Print *p, double num, int digits) {
    size_t n = print_float(p, num, digits);
    return n + println(p);
}
