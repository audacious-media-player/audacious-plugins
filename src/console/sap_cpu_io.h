
#include "Sap_Emu.h"

#include "blargg_source.h"

#define CPU_WRITE( cpu, addr, data, time )  STATIC_CAST(Sap_Emu&,*cpu).cpu_write( addr, data )
#define CPU_READ( cpu, addr, time )         STATIC_CAST(Sap_Emu&,*cpu).cpu_read( addr )

void Sap_Emu::cpu_write( sap_addr_t addr, int data )
{
	mem.ram [addr] = data;
	if ( (addr >> 8) == 0xD2 )
		cpu_write_( addr, data );
}

	int Sap_Emu::cpu_read( sap_addr_t addr )
	{
		switch ( (addr & 0xFF1F) )
		{
		case 0xD000:
			debug_printf( "Unmapped read $%04X\n", addr );
			break;
		case 0xD014:
			return info.ntsc ? 0xf : 1;
		case 0xD40B:
		case 0xD41B:
			if ( time() > ( info.ntsc ? 262 : 312 ) * 114 )
				return 0;
			return time() / 228;
		default:
			break;
		}
		return mem.ram [addr];
	}
