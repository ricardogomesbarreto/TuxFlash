/* Test-only UEFI application. No Windows components, no operating system. */
static inline void port(unsigned short p, unsigned char b) { __asm__ volatile("outb %0, %1" :: "a"(b), "Nd"(p)); }
static const char *volatile boot_marker = "LUFUS_UEFI_BOOT_OK\r\n";
unsigned long long efi_main(void *image, void *system) {
    (void)image; (void)system;
    const char *s = boot_marker;
    while (*s) port(0x3f8, (unsigned char)*s++);
    port(0xf4, 0x10);
    for (;;) __asm__ volatile("hlt");
}

/* GNU-EFI 4 calls _entry; earlier crt objects call efi_main. */
unsigned long long _entry(void *, void *) __attribute__((alias("efi_main")));
