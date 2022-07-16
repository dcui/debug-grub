/* main.c - the normal mode main routine */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/kernel.h>
#include <grub/net.h>
#include <grub/normal.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/parser.h>
#include <grub/reader.h>
#include <grub/menu_viewer.h>
#include <grub/auth.h>
#include <grub/i18n.h>
#include <grub/charset.h>
#include <grub/script_sh.h>
#include <grub/bufio.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define GRUB_DEFAULT_HISTORY_SIZE	50

static int nested_level = 0;
int grub_normal_exit_level = 0;

void
grub_normal_free_menu (grub_menu_t menu)
{
  grub_menu_entry_t entry = menu->entry_list;

  while (entry)
    {
      grub_menu_entry_t next_entry = entry->next;
      grub_size_t i;

      if (entry->classes)
	{
	  struct grub_menu_entry_class *class;
	  for (class = entry->classes; class; class = class->next)
	    grub_free (class->name);
	  grub_free (entry->classes);
	}

      if (entry->args)
	{
	  for (i = 0; entry->args[i]; i++)
	    grub_free (entry->args[i]);
	  grub_free (entry->args);
	}

      grub_free ((void *) entry->id);
      grub_free ((void *) entry->users);
      grub_free ((void *) entry->title);
      grub_free ((void *) entry->sourcecode);
      grub_free (entry);
      entry = next_entry;
    }

  grub_free (menu);
  grub_env_unset_menu ();
}

/* Helper for read_config_file.  */
static grub_err_t
read_config_file_getline (char **line, int cont __attribute__ ((unused)),
			  void *data)
{
  grub_file_t file = data;

  while (1)
    {
      char *buf;

      *line = buf = grub_file_getline (file);
      if (! buf)
	return grub_errno;

      if (buf[0] == '#')
	grub_free (*line);
      else
	break;
    }

  return GRUB_ERR_NONE;
}

static grub_menu_t
read_config_file (const char *config)
{
  grub_file_t rawfile, file;
  char *old_file = 0, *old_dir = 0;
  char *config_dir, *ptr = 0;
  const char *ctmp;

  grub_menu_t newmenu;

  grub_printf("cdx: %s, line %d, config=%s\n", __func__, __LINE__, config);
  newmenu = grub_env_get_menu ();
  if (! newmenu)
    {
      newmenu = grub_zalloc (sizeof (*newmenu));
      if (! newmenu)
	return 0;

      grub_env_set_menu (newmenu);
    }

  grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  /* Try to open the config file.  */
  rawfile = grub_file_open (config, GRUB_FILE_TYPE_CONFIG);
  grub_printf("cdx: %s, line %d, %p\n", __func__, __LINE__, rawfile);
  if (! rawfile)
    return 0;

  file = grub_bufio_open (rawfile, 0);
  grub_printf("cdx: %s, line %d, file=%p\n", __func__, __LINE__, file);
  if (! file)
    {
      grub_file_close (rawfile);
      return 0;
    }

  ctmp = grub_env_get ("config_file");
  grub_printf("cdx: %s, line %d, ctmp=%s\n", __func__, __LINE__, ctmp);
  if (ctmp)
    old_file = grub_strdup (ctmp);
  ctmp = grub_env_get ("config_directory");
  if (ctmp)
    old_dir = grub_strdup (ctmp);
  if (*config == '(')
    {
      grub_env_set ("config_file", config);
      config_dir = grub_strdup (config);
    }
  else
    {
      /* $root is guranteed to be defined, otherwise open above would fail */
      config_dir = grub_xasprintf ("(%s)%s", grub_env_get ("root"), config);
      if (config_dir)
	grub_env_set ("config_file", config_dir);
    }
  if (config_dir)
    {
      ptr = grub_strrchr (config_dir, '/');
      if (ptr)
	*ptr = 0;
      grub_env_set ("config_directory", config_dir);
      grub_free (config_dir);
    }

  grub_env_export ("config_file");
  grub_env_export ("config_directory");

  grub_printf("cdx: %s, line %d, ctmp=%s\n", __func__, __LINE__, ctmp);
  while (1)
    {
      char *line;

      /* Print an error, if any.  */
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;

      if ((read_config_file_getline (&line, 0, file)) || (! line))
	break;

      grub_printf("cdx: %s, line %d, line_msg=%s\n", __func__, __LINE__, line);
      grub_normal_parse_line (line, read_config_file_getline, file);
      grub_free (line);
    }

  grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  if (old_file)
    grub_env_set ("config_file", old_file);
  else
    grub_env_unset ("config_file");
  if (old_dir)
    grub_env_set ("config_directory", old_dir);
  else
    grub_env_unset ("config_directory");
  grub_free (old_file);
  grub_free (old_dir);

  grub_file_close (file);
  grub_printf("cdx: %s, line %d\n", __func__, __LINE__);

  return newmenu;
}

/* Initialize the screen.  */
void
grub_normal_init_page (struct grub_term_output *term,
		       int y)
{
  grub_ssize_t msg_len;
  int posx;
  char *msg_formatted;
  grub_uint32_t *unicode_msg;
  grub_uint32_t *last_position;

  grub_term_cls (term);

  msg_formatted = grub_xasprintf (_("GNU GRUB  version %s"), PACKAGE_VERSION);
  if (!msg_formatted)
    return;

  msg_len = grub_utf8_to_ucs4_alloc (msg_formatted,
  				     &unicode_msg, &last_position);
  grub_free (msg_formatted);

  if (msg_len < 0)
    {
      return;
    }

  posx = grub_getstringwidth (unicode_msg, last_position, term);
  posx = ((int) grub_term_width (term) - posx) / 2;
  if (posx < 0)
    posx = 0;
  grub_term_gotoxy (term, (struct grub_term_coordinate) { posx, y });

  grub_print_ucs4 (unicode_msg, last_position, 0, 0, term);
  grub_putcode ('\n', term);
  grub_putcode ('\n', term);
  grub_free (unicode_msg);
}

static void
read_lists (const char *val)
{
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  if (! grub_no_modules)
    {
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      read_command_list (val);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      read_fs_list (val);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      read_crypto_list (val);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      read_terminal_list (val);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
    }
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  grub_gettext_reread_prefix (val);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
}

static char *
read_lists_hook (struct grub_env_var *var __attribute__ ((unused)),
		 const char *val)
{
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  read_lists (val);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
  return val ? grub_strdup (val) : NULL;
}

/* Read the config file CONFIG and execute the menu interface or
   the command line interface if BATCH is false.  */
void
grub_normal_execute (const char *config, int nested, int batch)
{
  grub_menu_t menu = 0;
  const char *prefix;

  grub_printf("cdx: %s, line %d, nested=%d, batch=%d, config=%s\n", __func__, __LINE__, nested, batch, config);

  if (! nested)
    {
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      prefix = grub_env_get ("prefix");
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      read_lists (prefix);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      grub_register_variable_hook ("prefix", NULL, read_lists_hook);
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
    }

  grub_boot_time ("Executing config file");

  if (config)
    {
      grub_printf("cdx: %s, line %d, config=%s\n", __func__, __LINE__, config);
      menu = read_config_file (config);

      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      /* Ignore any error.  */
      grub_errno = GRUB_ERR_NONE;
    }

  grub_boot_time ("Executed config file");

  if (! batch)
    {
      grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
      if (menu && menu->size)
	{

          grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
	  grub_boot_time ("Entering menu");
          grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
	  grub_show_menu (menu, nested, 0);
          grub_printf("cdx: %s, line %d\n", __func__, __LINE__);
	  if (nested)
	    grub_normal_free_menu (menu);
	}
    }
}

volatile unsigned char cdx;
static __inline unsigned char
grub_inb (unsigned short int port)
 {
   unsigned char _v;

   asm volatile ("inb %w1,%0":"=a" (_v):"Nd" (port));
   return _v;
 }

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

/* This starts the normal mode.  */
void
grub_enter_normal_mode (const char *config)
{
  grub_boot_time ("Entering normal mode");

  nested_level++;
  //cdx = grub_inb(0x70);
  grub_normal_execute (config, 0, 0);

  //asm volatile("int $0x3");
  cdx_print("\n---------------------------------COM2 ~~~~~~~~~~~~~~~~~~~~------------------------------------\n");
  cdx = grub_inb(0x2f8); //COM2
  grub_printf("\ngrub_enter_normal_mode: inb_0x2f8=0x%x\n", cdx);
  grub_printf("\ngrub_enter_normal_mode: &grub_enter_normal_mode_func=0x%p\n", &grub_enter_normal_mode);

  //cdx = grub_inb(0x71);
  grub_boot_time ("Entering shell\n");
  grub_cmdline_run (0, 1);
  cdx = grub_inb(0x72);
  nested_level--;
  if (grub_normal_exit_level)
    grub_normal_exit_level--;
  grub_boot_time ("Exiting normal mode\n");
}

/* Enter normal mode from rescue mode.  */
static grub_err_t
grub_cmd_normal (struct grub_command *cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  //cdx = grub_inb(0x100+argc);
  //asm volatile("int $0x3");

  grub_printf("\ngrub_cmd_normal: 1:  argc=%d\n", argc);
  if (argc == 0)
    {
      /* Guess the config filename. It is necessary to make CONFIG static,
	 so that it won't get broken by longjmp.  */
      char *config;
      const char *prefix;

      prefix = grub_env_get ("prefix");
      grub_printf("\ngrub_cmd_normal: 2:  argc=%d, prefix=%s\n", argc, prefix);

      if (prefix)
        {
          grub_size_t config_len;
          int disable_net_search = 0;
          const char *net_search_cfg;

          config_len = grub_strlen (prefix) +
                       sizeof ("/grub.cfg-XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
          config = grub_malloc (config_len);
          grub_printf("\ngrub_cmd_normal: 3:  argc=%d, prefix=%s, config_len=%ld, config=%p\n", argc, prefix, config_len, config);


          if (!config)
            goto quit;

          grub_snprintf (config, config_len, "%s/grub.cfg", prefix);
          grub_printf("\ngrub_cmd_normal: 4:  argc=%d, prefix=%s, config_len=%ld, config=%s\n", argc, prefix, config_len, config);

          net_search_cfg = grub_env_get ("feature_net_search_cfg");
          grub_printf("\ngrub_cmd_normal: 5:  argc=%d, prefix=%s, config_len=%ld, config=%s, net=%s\n", argc, prefix, config_len, config, net_search_cfg);

          if (net_search_cfg && net_search_cfg[0] == 'n')
            disable_net_search = 1;

          if (grub_strncmp (prefix + 1, "tftp", sizeof ("tftp") - 1) == 0 &&
              !disable_net_search) {
            grub_printf("\ngrub_cmd_normal: 6.1:  argc=%d\n", argc);
            grub_net_search_config_file (config);
            grub_printf("\ngrub_cmd_normal: 6.2:  argc=%d\n", argc);
	  }

          grub_printf("\ngrub_cmd_normal: 7.1:  argc=%d\n", argc);
	  grub_enter_normal_mode (config);
          grub_printf("\ngrub_cmd_normal: 7.2:  argc=%d\n", argc);
	  grub_free (config);
	}
      else {
        grub_printf("\ngrub_cmd_normal: 8.1:  argc=%d\n", argc);
	grub_enter_normal_mode (0);
        grub_printf("\ngrub_cmd_normal: 8.2:  argc=%d\n", argc);
      }
    }
  else {
        grub_printf("\ngrub_cmd_normal: 9.1:  argc=%d\n", argc);
    grub_enter_normal_mode (argv[0]);
        grub_printf("\ngrub_cmd_normal: 9.1:  argc=%d\n", argc);
  }

quit:
  return 0;
}

/* Exit from normal mode to rescue mode.  */
static grub_err_t
grub_cmd_normal_exit (struct grub_command *cmd __attribute__ ((unused)),
		      int argc __attribute__ ((unused)),
		      char *argv[] __attribute__ ((unused)))
{
  if (nested_level <= grub_normal_exit_level)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "not in normal environment");
  grub_normal_exit_level++;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_normal_reader_init (int nested)
{
  struct grub_term_output *term;
  const char *msg_esc = _("ESC at any time exits.");
  char *msg_formatted;

  msg_formatted = grub_xasprintf (_("Minimal BASH-like line editing is supported. For "
				    "the first word, TAB lists possible command completions. Anywhere "
				    "else TAB lists possible device or file completions. %s"),
				  nested ? msg_esc : "");
  if (!msg_formatted)
    return grub_errno;

  FOR_ACTIVE_TERM_OUTPUTS(term)
  {
    grub_normal_init_page (term, 1);
    grub_term_setcursor (term, 1);

    if (grub_term_width (term) > 3 + STANDARD_MARGIN + 20)
      grub_print_message_indented (msg_formatted, 3, STANDARD_MARGIN, term);
    else
      grub_print_message_indented (msg_formatted, 0, 0, term);
    grub_putcode ('\n', term);
    grub_putcode ('\n', term);
    grub_putcode ('\n', term);
  }
  grub_free (msg_formatted);

  return 0;
}

static grub_err_t
grub_normal_read_line_real (char **line, int cont, int nested)
{
  const char *prompt;

  if (cont)
    /* TRANSLATORS: it's command line prompt.  */
    prompt = _(">");
  else
    /* TRANSLATORS: it's command line prompt.  */
    prompt = _("grub>");

  grub_printf("\ncdx: grub_normal_read_line_real: 1: cont=%d, nested=%d, promot=%s\n", cont, nested, prompt);
  if (!prompt)
    return grub_errno;

  grub_printf("cdx: grub_normal_read_line_real: 2: cont=%d, nested=%d, promot=%s\n", cont, nested, prompt);

  while (1)
    {
       grub_printf("cdx: grub_normal_read_line_real: 3.1: line=0x%p, *line=0x%p\n", line, *line);
      *line = grub_cmdline_get (prompt);
       grub_printf("cdx: grub_normal_read_line_real: 3.2: line=0x%p, *line=0x%p\n", line, *line);

      if (*line)
	return 0;

      grub_printf("cdx: grub_normal_read_line_real: 3.3: cont=%d, nested=%d, promot=%s\n", cont, nested, prompt);
      if (cont || nested)
	{
	  grub_free (*line);
	  *line = 0;
	  return grub_errno;
	}
    }

}

static grub_err_t
grub_normal_read_line (char **line, int cont,
		       void *data __attribute__ ((unused)))
{
  return grub_normal_read_line_real (line, cont, 0);
}

void
grub_cmdline_run (int nested, int force_auth)
{
  grub_err_t err = GRUB_ERR_NONE;

  cdx_print("\n--------------------------grub_cmdline_run-----------------\n");
  do
    {
      err = grub_auth_check_authentication (NULL);
      grub_printf("\n--------------------------grub_cmdline_run: cdx: 1: nested=%d, force_auth=%d -----------------\n", nested, force_auth);
      grub_printf("cdx: grub_cmdline_run: err=%d\n", err);
    }
  while (err && force_auth);

  cdx_print("\ncdx: grub_cmdline_run: 3\n");
  if (err)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  cdx_print("\ncdx: grub_cmdline_run: 4\n");
  grub_normal_reader_init (nested);
  cdx_print("\ncdx: grub_cmdline_run: 5\n");

  while (1)
    {
      char *line = NULL;

      cdx_print("\ncdx: grub_cmdline_run: 6.1\n");
      if (grub_normal_exit_level)
	break;

      cdx_print("\ncdx: grub_cmdline_run: 6.2\n");
      /* Print an error, if any.  */
      grub_print_error ();
      cdx_print("\ncdx: grub_cmdline_run: 6.3\n");
      grub_errno = GRUB_ERR_NONE;

      grub_normal_read_line_real (&line, 0, nested);
      cdx_print("\ncdx: grub_cmdline_run: 6.4\n");
      if (! line)
	break;

      cdx_print("\ncdx: grub_cmdline_run: 6.5\n");
      grub_normal_parse_line (line, grub_normal_read_line, NULL);
      cdx_print("\ncdx: grub_cmdline_run: 6.6\n");
      grub_free (line);
      cdx_print("\ncdx: grub_cmdline_run: 6.7\n");
    }
    cdx_print("\ncdx: grub_cmdline_run: 7\n");
}

static char *
grub_env_write_pager (struct grub_env_var *var __attribute__ ((unused)),
		      const char *val)
{
  grub_set_more ((*val == '1'));
  return grub_strdup (val);
}

/* clear */
static grub_err_t
grub_mini_cmd_clear (struct grub_command *cmd __attribute__ ((unused)),
		   int argc __attribute__ ((unused)),
		   char *argv[] __attribute__ ((unused)))
{
  grub_cls ();
  return 0;
}

static grub_command_t cmd_clear;

//static void (*grub_xputs_saved) (const char *str);
static const char *features[] = {
  "feature_chainloader_bpb", "feature_ntldr", "feature_platform_search_hint",
  "feature_default_font_path", "feature_all_video_module",
  "feature_menuentry_id", "feature_menuentry_options", "feature_200_final",
  "feature_nativedisk_cmd", "feature_timeout_style"
};

GRUB_MOD_INIT(normal)
{
  unsigned i;

  grub_boot_time ("Preparing normal module");

  /* Previously many modules depended on gzio. Be nice to user and load it.  */
  grub_dl_load ("gzio");
  grub_errno = 0;

  grub_normal_auth_init ();
  grub_context_init ();
  grub_script_init ();
  grub_menu_init ();

  //grub_xputs_saved = grub_xputs;
  //grub_xputs = grub_xputs_normal;
  grub_xputs = cdx_print;

  /* Normal mode shouldn't be unloaded.  */
  if (mod)
    grub_dl_ref (mod);

  cmd_clear =
    grub_register_command ("clear", grub_mini_cmd_clear,
			   0, N_("Clear the screen."));

  grub_set_history (GRUB_DEFAULT_HISTORY_SIZE);

  grub_register_variable_hook ("pager", 0, grub_env_write_pager);
  grub_env_export ("pager");

  /* Register a command "normal" for the rescue mode.  */
  grub_register_command ("normal", grub_cmd_normal,
			 0, N_("Enter normal mode."));
  grub_register_command ("normal_exit", grub_cmd_normal_exit,
			 0, N_("Exit from normal mode."));

  /* Reload terminal colors when these variables are written to.  */
  grub_register_variable_hook ("color_normal", NULL, grub_env_write_color_normal);
  grub_register_variable_hook ("color_highlight", NULL, grub_env_write_color_highlight);

  /* Preserve hooks after context changes.  */
  grub_env_export ("color_normal");
  grub_env_export ("color_highlight");

  /* Set default color names.  */
  grub_env_set ("color_normal", "light-gray/black");
  grub_env_set ("color_highlight", "black/light-gray");

  for (i = 0; i < ARRAY_SIZE (features); i++)
    {
      grub_env_set (features[i], "y");
      grub_env_export (features[i]);
    }
  grub_env_set ("grub_cpu", GRUB_TARGET_CPU);
  grub_env_export ("grub_cpu");
  grub_env_set ("grub_platform", GRUB_PLATFORM);
  grub_env_export ("grub_platform");

  grub_boot_time ("Normal module prepared");
}

GRUB_MOD_FINI(normal)
{
  grub_context_fini ();
  grub_script_fini ();
  grub_menu_fini ();
  grub_normal_auth_fini ();

  //grub_xputs = grub_xputs_saved;

  grub_set_history (0);
  grub_register_variable_hook ("pager", 0, 0);
  grub_fs_autoload_hook = 0;
  grub_unregister_command (cmd_clear);
}
