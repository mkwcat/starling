OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc");
OUTPUT_ARCH(powerpc:common);
__entry = 0;
ENTRY(__entry)

PHDRS
{
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
	imports PT_LOAD;
	bss PT_LOAD;
}

SECTIONS
{
	. = 0x81600000;

	.text : {
		KEEP (*(.start))
		*(.text*)
		*(.fimport.*)
	} : text = 0

	.data : {
		. = ALIGN(4);
		TwmTable = .;
		*(.twm_table*)
		KEEP (*(.twm_table_strong))
		TwmTableEnd = .;

		. = ALIGN(4);
		_ctors_start = .;
		KEEP( *(.ctors) )
		_ctors_end = .;

		. = ALIGN(32);
		*(.rodata*)
		*(.data*)
		*(.sdata*)
		. = ALIGN(32);
	} : data = 0

	.bss : {
		PROVIDE(_bss_start = .);
		*(.sbss)
		*(.sbss.*)
		*(.sbss2)
		*(.sbss2.*)
		*(.bss)
		*(.bss.*)
		. = ALIGN(32);
		PROVIDE (_bss_end = .);
	} : bss = 0

	/DISCARD/ : {
		*(.comment)
	}

	vtr = 0xcc002000;
    dcr = 0xcc002002;
    vto = 0xcc00200c;
    vte = 0xcc002010;
    tfbl = 0xcc00201c;
    bfbl = 0xcc002024;
    hsw = 0xcc002048;
    hsr = 0xcc00204a;
    visel = 0xcc00206e;

    WGPIPE = 0xcc008000;
}
