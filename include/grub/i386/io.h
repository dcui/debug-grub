/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1996,2000,2002,2007  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Based on sys/io.h from GNU libc. */

#ifndef	GRUB_IO_H
#define	GRUB_IO_H	1

typedef unsigned short int grub_port_t;

/////////////////cuidexuan cdx
#if 1
static __inline int is_uart_port(unsigned short int port)
{
	//0x03F8-0x03FF	First serial port
	//0x02F8-0x02FF	Second serial port

	if (port >= 0x03F8 && port <= 0x03FF) {
		asm volatile("int $0x3");
		return 1;
	}

	if (port >= 0x02F8 && port <= 0x02FF) {
		asm volatile("int $0x3");
		return 1;
	}

	return 0;
}
#else
#define is_uart_port(port) 0
#endif

static __inline unsigned char
grub_inb (unsigned short int port)
{
  unsigned char _v;

  if (!is_uart_port(port)) return 0xFF;
  asm volatile ("inb %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline unsigned short int
grub_inw (unsigned short int port)
{
  unsigned short _v;

  if (!is_uart_port(port)) return 0xFFFF;
  asm volatile ("inw %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline unsigned int
grub_inl (unsigned short int port)
{
  unsigned int _v;

  if (!is_uart_port(port)) return 0xFFFFFFFF;
  asm volatile ("inl %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline void
grub_outb (unsigned char value, unsigned short int port)
{
  if (!is_uart_port(port)) return;
  asm volatile ("outb %b0,%w1": :"a" (value), "Nd" (port));
}

static __inline void
grub_outw (unsigned short int value, unsigned short int port)
{
  if (!is_uart_port(port)) return;
  asm volatile ("outw %w0,%w1": :"a" (value), "Nd" (port));

}

static __inline void
grub_outl (unsigned int value, unsigned short int port)
{
  if (!is_uart_port(port)) return;
  asm volatile ("outl %0,%w1": :"a" (value), "Nd" (port));
}

#endif /* _SYS_IO_H */
