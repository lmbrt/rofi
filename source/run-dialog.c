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
#include <stdlib.h>
#include <stdio.h>
#include <X11/X.h>

#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

#include "rofi.h"
#include "helper.h"
#include "history.h"
#include "run-dialog.h"
#ifdef TIMING
#include <time.h>
#endif

#define RUN_CACHE_FILE    "rofi-2.runcache"

static inline void execsh ( const char *cmd, int run_in_term )
{
    char **args = NULL;
    int  argc   = 0;
    if ( run_in_term ) {
        helper_parse_setup ( config.run_shell_command, &args, &argc, "{cmd}", cmd, NULL );
    }
    else {
        helper_parse_setup ( config.run_command, &args, &argc, "{cmd}", cmd, NULL );
    }
    GError *error = NULL;
    g_spawn_async ( NULL, args, NULL,
                    G_SPAWN_SEARCH_PATH,
                    NULL, NULL, NULL, &error );
    if ( error != NULL ) {
        char *msg = g_strdup_printf ( "Failed to execute: '%s'\nError: '%s'", cmd,
                                      error->message );
        error_dialog ( msg );
        g_free ( msg );
        // print error.
        g_error_free ( error );
    }

    // Free the args list.
    g_strfreev ( args );
}

// execute sub-process
static void exec_cmd ( const char *cmd, int run_in_term )
{
    if ( !cmd || !cmd[0] ) {
        return;
    }


    execsh ( cmd, run_in_term );

    /**
     * This happens in non-critical time (After launching app)
     * It is allowed to be a bit slower.
     */
    char *path = g_strdup_printf ( "%s/%s", cache_dir, RUN_CACHE_FILE );

    history_set ( path, cmd );

    g_free ( path );
}
// execute sub-process
static void delete_entry ( const char *cmd )
{
    /**
     * This happens in non-critical time (After launching app)
     * It is allowed to be a bit slower.
     */
    char *path = g_strdup_printf ( "%s/%s", cache_dir, RUN_CACHE_FILE );

    history_remove ( path, cmd );

    g_free ( path );
}
static int sort_func ( const void *a, const void *b )
{
    const char *astr = *( const char * const * ) a;
    const char *bstr = *( const char * const * ) b;

    if ( astr == NULL && bstr == NULL ) {
        return 0;
    }
    else if ( astr == NULL ) {
        return 1;
    }
    else if ( bstr == NULL ) {
        return -1;
    }
    return strcasecmp ( astr, bstr );
}

/**
 * External spider to get list of executables.
 */
static char ** get_apps_external ( char **retv, unsigned int *length, unsigned int num_favorites )
{
    int fd = execute_generator ( config.run_list_command );
    if ( fd >= 0 ) {
        FILE *inp = fdopen ( fd, "r" );
        if ( inp ) {
            char buffer[1024];
            while ( fgets ( buffer, 1024, inp ) != NULL ) {
                int found = 0;
                // Filter out line-end.
                if ( buffer[strlen ( buffer ) - 1] == '\n' ) {
                    buffer[strlen ( buffer ) - 1] = '\0';
                }

                // This is a nice little penalty, but doable? time will tell.
                // given num_favorites is max 25.
                for ( unsigned int j = 0; found == 0 && j < num_favorites; j++ ) {
                    if ( strcasecmp ( buffer, retv[j] ) == 0 ) {
                        found = 1;
                    }
                }

                if ( found == 1 ) {
                    continue;
                }

                // No duplicate, add it.
                retv                  = g_realloc ( retv, ( ( *length ) + 2 ) * sizeof ( char* ) );
                retv[( *length )]     = g_strdup ( buffer );
                retv[( *length ) + 1] = NULL;


                ( *length )++;
            }
            fclose ( inp );
        }
    }
    return retv;
}

/**
 * Internal spider used to get list of executables.
 */
static char ** get_apps ( char **retv, unsigned int *length )
{
    unsigned int    num_favorites = 0;
    char            *path;
#ifdef TIMING
    struct timespec start, stop;
    clock_gettime ( CLOCK_REALTIME, &start );
#endif

    if ( getenv ( "PATH" ) == NULL ) {
        return NULL;
    }


    path = g_strdup_printf ( "%s/%s", cache_dir, RUN_CACHE_FILE );
    retv = history_get_list ( path, length );
    g_free ( path );
    // Keep track of how many where loaded as favorite.
    num_favorites = ( *length );


    path = g_strdup ( getenv ( "PATH" ) );

    for ( const char *dirname = strtok ( path, ":" );
          dirname != NULL;
          dirname = strtok ( NULL, ":" ) ) {
        DIR *dir = opendir ( dirname );

        if ( dir != NULL ) {
            struct dirent *dent;

            while ( ( dent = readdir ( dir ) ) != NULL ) {
                if ( dent->d_type != DT_REG &&
                     dent->d_type != DT_LNK &&
                     dent->d_type != DT_UNKNOWN ) {
                    continue;
                }
                // Skip dot files.
                if ( dent->d_name[0] == '.' ) {
                    continue;
                }

                int found = 0;

                // This is a nice little penalty, but doable? time will tell.
                // given num_favorites is max 25.
                for ( unsigned int j = 0; found == 0 && j < num_favorites; j++ ) {
                    if ( strcasecmp ( dent->d_name, retv[j] ) == 0 ) {
                        found = 1;
                    }
                }

                if ( found == 1 ) {
                    continue;
                }

                retv                  = g_realloc ( retv, ( ( *length ) + 2 ) * sizeof ( char* ) );
                retv[( *length )]     = g_strdup ( dent->d_name );
                retv[( *length ) + 1] = NULL;
                ( *length )++;
            }

            closedir ( dir );
        }
    }

    // Get external apps.
    if ( config.run_list_command != NULL && config.run_list_command[0] != '\0' ) {
        retv = get_apps_external ( retv, length, num_favorites );
    }
    // No sorting needed.
    if ( ( *length ) == 0 ) {
        return retv;
    }
    // TODO: check this is still fast enough. (takes 1ms on laptop.)
    if ( ( *length ) > num_favorites ) {
        qsort ( &retv[num_favorites], ( *length ) - num_favorites, sizeof ( char* ), sort_func );
    }
    g_free ( path );
#ifdef TIMING
    {
        clock_gettime ( CLOCK_REALTIME, &stop );

        if ( stop.tv_sec != start.tv_sec ) {
            stop.tv_nsec += ( stop.tv_sec - start.tv_sec ) * 1e9;
        }

        long diff = stop.tv_nsec - start.tv_nsec;
        printf ( "Time elapsed: %ld us\n", diff / 1000 );
    }
#endif

    unsigned int removed = 0;
    for ( unsigned int index = num_favorites; index < ( ( *length ) - 1 ); index++ ) {
        if ( strcmp ( retv[index], retv[index + 1] ) == 0 ) {
            g_free ( retv[index] );
            retv[index] = NULL;
            removed++;
        }
    }


    if ( ( *length ) > num_favorites ) {
        qsort ( &retv[num_favorites], ( *length ) - num_favorites, sizeof ( char* ), sort_func );
    }
    // Reduce array length;
    ( *length ) -= removed;

#ifdef TIMING
    {
        clock_gettime ( CLOCK_REALTIME, &stop );

        if ( stop.tv_sec != start.tv_sec ) {
            stop.tv_nsec += ( stop.tv_sec - start.tv_sec ) * 1e9;
        }

        long diff = stop.tv_nsec - start.tv_nsec;
        printf ( "Time elapsed: %ld us\n", diff / 1000 );
    }
#endif
    return retv;
}

SwitcherMode run_switcher_dialog ( char **input, G_GNUC_UNUSED void *data )
{
    int          shift         = 0;
    int          selected_line = 0;
    SwitcherMode retv          = MODE_EXIT;
    // act as a launcher
    unsigned int cmd_list_length = 0;
    char         **cmd_list      = NULL;
    cmd_list = get_apps ( cmd_list, &cmd_list_length );

    if ( cmd_list == NULL ) {
        cmd_list    = g_malloc_n ( 2, sizeof ( char * ) );
        cmd_list[0] = g_strdup ( "No applications found" );
        cmd_list[1] = NULL;
    }

    int mretv = menu ( cmd_list, cmd_list_length, input, "run:",
                       NULL, &shift, token_match, NULL, &selected_line,
                       config.levenshtein_sort );

    if ( mretv == MENU_NEXT ) {
        retv = NEXT_DIALOG;
    }
    else if ( mretv == MENU_PREVIOUS ) {
        retv = PREVIOUS_DIALOG;
    }
    else if ( mretv == MENU_QUICK_SWITCH ) {
        retv = selected_line;
    }
    else if ( mretv == MENU_OK && cmd_list[selected_line] != NULL ) {
        exec_cmd ( cmd_list[selected_line], shift );
    }
    else if ( mretv == MENU_CUSTOM_INPUT && *input != NULL && *input[0] != '\0' ) {
        exec_cmd ( *input, shift );
    }
    else if ( mretv == MENU_ENTRY_DELETE && cmd_list[selected_line] ) {
        delete_entry ( cmd_list[selected_line] );
        retv = RELOAD_DIALOG;
    }

    g_strfreev ( cmd_list );

    return retv;
}
