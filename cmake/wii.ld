OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)
ENTRY(__app_start)

PROVIDE_HIDDEN( __isIPL = 0 );
PROVIDE_HIDDEN( __ppc_excpt_buf   = 0x800000d0 );
PROVIDE_HIDDEN( __ppc_excpt_table = 0x80003000 );

PHDRS {
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
	bss  PT_LOAD;
}

SECTIONS {

	. = 0x80003100;

	.text : {
		_start = .;
		*(.crt0 .crt0.*)
		*(.init)
		*(.text .text.* .gnu.linkonce.t.* .plt .stub .glink .iplt)
	} :text

	. = ALIGN(32);

	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.* .rodata1 .fixup .got1 .got2 .branch_lt .got .plt)
	} :data

	.sdata2 : {
		*(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
	} :data

	.sbss2 : {
		*(.sbss2 .sbss2.* .gnu.linkonce.sb2.*)
	} :data

	.ctors : {
		KEEP( *ecrti.o(.ctors) )
		KEEP( *crtbegin.o(.ctors) )
		KEEP( *crtbegin?.o(.ctors) )
		KEEP( *(.ctors) )
	} :data

	.dtors : {
		KEEP( *ecrti.o(.dtors) )
		KEEP( *crtbegin.o(.dtors) )
		KEEP( *crtbegin?.o(.dtors) )
		KEEP( *(.dtors) )
	} :data

	.data : {
		*(.data .data.* .gnu.linkonce.d.* .data1)
		SORT(CONSTRUCTORS)
	} :data

	.sdata : {
		*(.sdata .sdata.* .gnu.linkonce.s.* .lit8 .lit4)
	} :data

	.sbss : {
		*(.dynsbss .sbss .sbss.* .gnu.linkonce.sb.* .scommon)
	} :bss

	PROVIDE_HIDDEN( __sbss_start = ADDR(.sbss) );
	PROVIDE_HIDDEN( __sbss_end = ADDR(.sbss) + SIZEOF(.sbss) );

	.bss : {
		__bss_start = .;
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		__bss_end = .;
	} :bss

	PROVIDE_HIDDEN( __Arena1Lo = (. + 0x1f) &~ 0x1f );
	PROVIDE_HIDDEN( __Arena1Hi = 0x81800000 );
	PROVIDE_HIDDEN( __Arena2Lo = 0x81800000 );
	PROVIDE_HIDDEN( __Arena2Hi = 0x82000000 );
	PROVIDE_HIDDEN( __ipcbufferLo = 0x93000000 );
	PROVIDE_HIDDEN( __ipcbufferHi = 0x93002000 );

	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }
}
