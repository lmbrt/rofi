/**
 * rofi
 *
 * MIT/X11 License
 * Copyright 2013-2014 Qball  Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xresource.h>
#include "rofi.h"
#include "xrmoptions.h"

// Big thanks to Sean Pringle for this code.
// This maps xresource options to config structure.
typedef enum
{
    xrm_String  = 0,
    xrm_Number  = 1,
    xrm_SNumber = 2,
    xrm_Boolean = 3
} XrmOptionType;

typedef struct
{
    int  type;
    char * name;
    union
    {
        unsigned int * num;
        int          * snum;
        char         ** str;
    };
    char *mem;
} XrmOption;
/**
 * Map X resource settings to internal options
 * Currently supports string and number.
 */
static XrmOption xrmOptions[] = {
    { xrm_String,  "switchers",            { .str  = &config.switchers             }, NULL },
    { xrm_Number,  "opacity",              { .num  = &config.window_opacity        }, NULL },

    { xrm_SNumber, "width",                { .snum = &config.menu_width            }, NULL },

    { xrm_Number,  "lines",                { .num  = &config.menu_lines            }, NULL },
    { xrm_Number,  "columns",              { .num  = &config.menu_columns          }, NULL },

    { xrm_String,  "font",                 { .str  = &config.menu_font             }, NULL },
    /* Foreground color */
    { xrm_String,  "foreground",           { .str  = &config.menu_fg               }, NULL },
    { xrm_String,  "fg",                   { .str  = &config.menu_fg               }, NULL },

    { xrm_String,  "background",           { .str  = &config.menu_bg               }, NULL },
    { xrm_String,  "bg",                   { .str  = &config.menu_bg               }, NULL },
    { xrm_String,  "background-alternate", { .str  = &config.menu_bg_alt           }, NULL },
    { xrm_String,  "bgalt",                { .str  = &config.menu_bg_alt           }, NULL },

    { xrm_String,  "highlightfg",          { .str  = &config.menu_hlfg             }, NULL },
    { xrm_String,  "hlfg",                 { .str  = &config.menu_hlfg             }, NULL },

    { xrm_String,  "highlightbg",          { .str  = &config.menu_hlbg             }, NULL },
    { xrm_String,  "hlbg",                 { .str  = &config.menu_hlbg             }, NULL },

    { xrm_String,  "bordercolor",          { .str  = &config.menu_bc               }, NULL },
    { xrm_String,  "bc",                   { .str  = &config.menu_bc               }, NULL },

    { xrm_Number,  "borderwidth",          { .num  = &config.menu_bw               }, NULL },
    { xrm_Number,  "bw",                   { .num  = &config.menu_bw               }, NULL },

    { xrm_Number,  "location",             { .num  = &config.location              }, NULL },

    { xrm_Number,  "padding",              { .num  = &config.padding               }, NULL },
    { xrm_SNumber, "yoffset",              { .snum = &config.y_offset              }, NULL },
    { xrm_SNumber, "xoffset",              { .snum = &config.x_offset              }, NULL },
    { xrm_Boolean, "fixed-num-lines",      { .num  = &config.fixed_num_lines       }, NULL },

    { xrm_String,  "terminal",             { .str  = &config.terminal_emulator     }, NULL },
    { xrm_String,  "ssh-client",           { .str  = &config.ssh_client            }, NULL },
    { xrm_String,  "ssh-command",          { .str  = &config.ssh_command           }, NULL },
    { xrm_String,  "run-command",          { .str  = &config.run_command           }, NULL },
    { xrm_String,  "run-list-command",     { .str  = &config.run_list_command      }, NULL },
    { xrm_String,  "run-shell-command",    { .str  = &config.run_shell_command     }, NULL },

    { xrm_Boolean, "disable-history",      { .num  = &config.disable_history       }, NULL },
    { xrm_Boolean, "levenshtein-sort",     { .num  = &config.levenshtein_sort      }, NULL },
    { xrm_Boolean, "case-sensitive",       { .num  = &config.case_sensitive        }, NULL },
    /* Key bindings */
    { xrm_String,  "key",                  { .str  = &config.window_key            }, NULL },
    { xrm_String,  "rkey",                 { .str  = &config.run_key               }, NULL },
    { xrm_String,  "skey",                 { .str  = &config.ssh_key               }, NULL },
    { xrm_Boolean, "sidebar-mode",         { .num  = &config.sidebar_mode          }, NULL },
    { xrm_Number,  "lazy-filter-limit",    { .num  = &config.lazy_filter_limit     }, NULL }
};


void config_parse_xresource_options ( Display *display )
{
    char *xRMS;
    // Map Xresource entries to rofi config options.
    XrmInitialize ();
    xRMS = XResourceManagerString ( display );

    if ( xRMS == NULL ) {
        return;
    }
    XrmDatabase xDB = XrmGetStringDatabase ( xRMS );

    char        * xrmType;
    XrmValue    xrmValue;
    const char  * namePrefix  = "rofi";
    const char  * classPrefix = "rofi";

    for ( unsigned int i = 0; i < sizeof ( xrmOptions ) / sizeof ( *xrmOptions ); ++i ) {
        char *name, *class;

        name  = g_strdup_printf ( "%s.%s", namePrefix, xrmOptions[i].name );
        class = g_strdup_printf ( "%s.%s", classPrefix, xrmOptions[i].name );

        if ( XrmGetResource ( xDB, name, class, &xrmType, &xrmValue ) ) {
            if ( xrmOptions[i].type == xrm_String ) {
                if ( xrmOptions[i].mem != NULL ) {
                    g_free ( xrmOptions[i].mem );
                    xrmOptions[i].mem = NULL;
                }
                *xrmOptions[i].str = g_strndup ( xrmValue.addr, xrmValue.size );

                // Memory
                xrmOptions[i].mem = ( *xrmOptions[i].str );
            }
            else if ( xrmOptions[i].type == xrm_Number ) {
                *xrmOptions[i].num = (unsigned int) strtoul ( xrmValue.addr, NULL, 10 );
            }
            else if ( xrmOptions[i].type == xrm_SNumber ) {
                *xrmOptions[i].snum = (int) strtol ( xrmValue.addr, NULL, 10 );
            }
            else if ( xrmOptions[i].type == xrm_Boolean ) {
                if ( xrmValue.size > 0 && g_ascii_strncasecmp ( xrmValue.addr, "true", xrmValue.size ) == 0 ) {
                    *xrmOptions[i].num = TRUE;
                }
                else{
                    *xrmOptions[i].num = FALSE;
                }
            }
        }

        g_free ( class );
        g_free ( name );
    }
    XrmDestroyDatabase ( xDB );
}

void config_xresource_free ( void )
{
    for ( unsigned int i = 0; i < sizeof ( xrmOptions ) / sizeof ( *xrmOptions ); ++i ) {
        if ( xrmOptions[i].mem != NULL ) {
            g_free ( xrmOptions[i].mem );
            xrmOptions[i].mem = NULL;
        }
    }
}

void xresource_dump ( void )
{
    const char   * namePrefix = "rofi";
    unsigned int entries      = sizeof ( xrmOptions ) / sizeof ( *xrmOptions );
    for ( unsigned int i = 0; i < entries; ++i ) {
        // Skip duplicates.
        if ( ( i + 1 ) < entries ) {
            if ( xrmOptions[i].str == xrmOptions[i + 1].str ) {
                continue;
            }
        }

        printf ( "%s.%s: %*s", namePrefix, xrmOptions[i].name, (int) ( 20 - strlen ( xrmOptions[i].name ) ),
                 "" );
        switch ( xrmOptions[i].type )
        {
        case xrm_Number:
            printf ( "%u", *xrmOptions[i].num );
            break;
        case xrm_SNumber:
            printf ( "%i", *xrmOptions[i].snum );
            break;
        case xrm_String:
            printf ( "%s", *xrmOptions[i].str );
            break;
        case xrm_Boolean:
            printf ( "%s", ( ( *xrmOptions[i].num ) == TRUE ) ? "true" : "false" );
            break;
        default:
            break;
        }
        printf ( "\n" );
    }
}
