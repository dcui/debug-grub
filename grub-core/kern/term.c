/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2005,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/term.h>
#include <grub/err.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/env.h>
#include <grub/time.h>

struct grub_term_output *grub_term_outputs_disabled;
struct grub_term_input *grub_term_inputs_disabled;
struct grub_term_output *grub_term_outputs;
struct grub_term_input *grub_term_inputs;

/* Current color state.  */
grub_uint8_t grub_term_normal_color = GRUB_TERM_DEFAULT_NORMAL_COLOR;
grub_uint8_t grub_term_highlight_color = GRUB_TERM_DEFAULT_HIGHLIGHT_COLOR;

void (*grub_term_poll_usb) (int wait_for_completion) = NULL;
void (*grub_net_poll_cards_idle) (void) = NULL;

#if 0
/* Put a Unicode character.  */
static void
grub_putcode_dumb (grub_uint32_t code,
		   struct grub_term_output *term)
{
  struct grub_unicode_glyph c =
    {
      .base = code,
      .variant = 0,
      .attributes = 0,
      .ncomb = 0,
      .estimated_width = 1
    };

  if (code == '\t' && term->getxy)
    {
      int n;

      n = GRUB_TERM_TAB_WIDTH - ((term->getxy (term).x)
				 % GRUB_TERM_TAB_WIDTH);
      while (n--)
	grub_putcode_dumb (' ', term);

      return;
    }

  (term->putchar) (term, &c);
  if (code == '\n')
    grub_putcode_dumb ('\r', term);
}

static void
grub_xputs_dumb (const char *str)
{
  for (; *str; str++)
    {
      grub_term_output_t term;
      grub_uint32_t code = *str;
      if (code > 0x7f)
	code = '?';

      FOR_ACTIVE_TERM_OUTPUTS(term)
	grub_putcode_dumb (code, term);
    }
}
#else
/////////////////////////////////////////////////////////////////////////////////////////////
typedef unsigned int u32;
typedef unsigned long long u64;

#define TDX_HYPERCALL_STANDARD  0
#define tdcall   ".byte 0x66,0x0f,0x01,0xcc"
#define EXIT_REASON_MSR_WRITE   32


static int cdx_write_msr(u64 str)
{
	int ret;

        asm volatile(
                "xor %%eax, %%eax\n\t"

		"movq $0, %%r10\n\t"
		"movq $32, %%r11\n\t"
		"movq $0x400000C1, %%r12\n\t"
		"movq %1, %%r13\n\t"

                "movl $0x3c00,%%ecx\n\t" //R10~R13

		tdcall "\n\t"

		"testq %%rax, %%rax\n\t"
		"je 2f\n\t"
"1:\t		int $0x3\n\t"
"2:\t		movq %%r10, %%rax\n\t"
		"testq %%rax, %%rax\n\t"
		"je 4f\n\t"
"3:\t		int $0x3\n\t"
"4:\t		\n\t"

                : "=&a" (ret)
		: "r" (str)
		: "%r10", "%r11", "%r12", "%r13", "ecx", "memory");

        return ret;
}

static void cdx_print(const char *msg)
{
	const char *p = msg;
	u64 ch;

	while (*p) {
		ch = *p;
		cdx_write_msr(ch);
		p++;
	}

	//return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
#endif

//void (*grub_xputs) (const char *str) = grub_xputs_dumb;
void (*grub_xputs) (const char *str) = cdx_print;

int
grub_getkey_noblock (void)
{
  grub_term_input_t term;

  grub_printf("cdx: %s, line %d, grub_term_poll_usb=%p, grub_net_poll_cards_idle=%p\n", __func__, __LINE__,
		grub_term_poll_usb, grub_net_poll_cards_idle);
  if (grub_term_poll_usb)
    grub_term_poll_usb (0);

  if (grub_net_poll_cards_idle)
    grub_net_poll_cards_idle ();

  FOR_ACTIVE_TERM_INPUTS(term)
  {
    int key = term->getkey (term);

    grub_printf("cdx: %s, line %d, term(%s, %s)=%p, key = %d\n", __func__, __LINE__,
		term->name, term->name2 ? term->name2:"[no term name2]", term, key);

    if (key != GRUB_TERM_NO_KEY)
      return key;
  }

  grub_printf("cdx: %s, line %d, key(error) = %d\n", __func__, __LINE__, 0);
  return GRUB_TERM_NO_KEY;
}

int
grub_getkey (void)
{
  int ret;
  grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  grub_refresh ();
  grub_printf("cdx: %s, line %d\n", __func__, __LINE__);

  while (1)
    {
      ret = grub_getkey_noblock ();
      grub_printf("cdx: %s, line %d, ret=%d\n", __func__, __LINE__, ret);
      if (ret != GRUB_TERM_NO_KEY)
	return ret;
      grub_cpu_idle ();
      grub_printf("cdx: %s, line %d, ret=%d\n", __func__, __LINE__, ret);
    }
}

int
grub_getkeystatus (void)
{
  int status = 0;
  grub_term_input_t term;

  if (grub_term_poll_usb)
    grub_term_poll_usb (0);

  FOR_ACTIVE_TERM_INPUTS(term)
  {
    if (term->getkeystatus)
      status |= term->getkeystatus (term);
  }

  return status;
}

int
grub_key_is_interrupt (int key)
{
  /*
   * ESC sometimes is the BIOS setup hotkey and may be hard to discover, also
   * check F4, which was chosen because is not used as a hotkey to enter the
   * BIOS setup by any vendor.
   */
  if (key == GRUB_TERM_ESC || key == GRUB_TERM_KEY_F4)
    return 1;

  /*
   * Pressing keys at the right time during boot is hard to time, also allow
   * interrupting sleeps / the menu countdown by keeping shift pressed.
   */
  if (grub_getkeystatus() & (GRUB_TERM_STATUS_LSHIFT | GRUB_TERM_STATUS_RSHIFT))
    return 1;

  return 0;
}

void
grub_refresh (void)
{
  struct grub_term_output *term;

  FOR_ACTIVE_TERM_OUTPUTS(term)
    grub_term_refresh (term);
}
