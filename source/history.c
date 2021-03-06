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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include "rofi.h"
#include "history.h"

#define HISTORY_NAME_LENGTH    256
#define HISTORY_MAX_ENTRIES    25

typedef struct __element
{
    long int index;
    char     name[HISTORY_NAME_LENGTH];
}_element;

static int __element_sort_func ( const void *ea, const void *eb )
{
    _element *a = *(_element * *) ea;
    _element *b = *(_element * *) eb;
    return b->index - a->index;
}

static void __history_write_element_list ( FILE *fd, _element **list, unsigned int length )
{
    if ( list == NULL || length == 0 ) {
        return;
    }
    // Sort the list before writing out.
    qsort ( list, length, sizeof ( _element* ), __element_sort_func );

    // Get minimum index.
    int min_value = list[length - 1]->index;

    // Set the max length of the list.
    length = ( length > HISTORY_MAX_ENTRIES ) ? HISTORY_MAX_ENTRIES : length;

    // Write out entries.
    for ( unsigned int iter = 0; iter < length; iter++ ) {
        fprintf ( fd, "%ld %s\n", list[iter]->index - min_value, list[iter]->name );
    }
}

static _element ** __history_get_element_list ( FILE *fd, unsigned int *length )
{
    char     buffer[HISTORY_NAME_LENGTH + 16];
    _element **retv = NULL;

    if  ( length == NULL ) {
        return NULL;
    }
    *length = 0;

    if ( fd == NULL ) {
        return NULL;
    }
    while ( fgets ( buffer, HISTORY_NAME_LENGTH + 16, fd ) != NULL ) {
        char * start = NULL;
        // Skip empty lines.
        if ( strlen ( buffer ) == 0 ) {
            continue;
        }
        // Resize and check.
        retv = g_realloc ( retv, ( *length + 2 ) * sizeof ( _element* ) );

        retv[( *length )] = g_malloc ( sizeof ( _element ) );
        // remove trailing \n
        buffer[strlen ( buffer ) - 1] = '\0';
        // Parse the number of times.
        retv[( *length )]->index = strtol ( buffer, &start, 10 );
        strncpy ( retv[( *length )]->name, ( start + 1 ), HISTORY_NAME_LENGTH );
        // Force trailing '\0'
        retv[( *length )]->name[HISTORY_NAME_LENGTH - 1] = '\0';
        retv[( *length ) + 1]                            = NULL;

        ( *length )++;
    }
    return retv;
}


void history_set ( const char *filename, const char *entry )
{
    if ( config.disable_history ) {
        return;
    }
    int          found  = 0;
    unsigned int curr   = 0;
    unsigned int length = 0;
    _element     **list = NULL;
    // Open file for reading and writing.
    FILE         *fd = fopen ( filename, "a+" );
    if ( fd == NULL ) {
        fprintf ( stderr, "Failed to open file: %s\n", strerror ( errno ) );
        return;
    }
    // Get list.
    list = __history_get_element_list ( fd, &length );

    // Look if the entry exists.
    for ( unsigned int iter = 0; !found && iter < length; iter++ ) {
        if ( strcmp ( list[iter]->name, entry ) == 0 ) {
            curr  = iter;
            found = 1;
        }
    }

    if ( found ) {
        // If exists, increment list index number
        list[curr]->index++;
    }
    else{
        // If not exists, add it.
        // Increase list by one
        list         = g_realloc ( list, ( length + 2 ) * sizeof ( _element* ) );
        list[length] = g_malloc ( sizeof ( _element ) );
        // Copy name
        if ( list[length] != NULL ) {
            g_strlcpy ( list[length]->name, entry, HISTORY_NAME_LENGTH );
            list[length]->name[HISTORY_NAME_LENGTH - 1] = '\0';
            // set # hits
            list[length]->index = 1;

            length++;
            list[length] = NULL;
        }
    }

    // Rewind.
    fseek ( fd, 0L, SEEK_SET );
    // Clear file.
    if ( ftruncate ( fileno ( fd ), 0 ) == 0 ) {
        // Write list.
        __history_write_element_list ( fd, list, length );
    }
    else{
        fprintf ( stderr, "Failed to truncate file: %s\n", strerror ( errno ) );
    }
    // Free the list.
    for ( unsigned int iter = 0; iter < length; iter++ ) {
        g_free ( list[iter] );
    }
    g_free ( list );
    // Close file.
    fclose ( fd );
}

void history_remove ( const char *filename, const char *entry )
{
    if ( config.disable_history ) {
        return;
    }
    _element     ** list = NULL;
    int          found   = 0;
    unsigned int curr    = 0;
    unsigned int length  = 0;
    // Open file for reading and writing.
    FILE         *fd = fopen ( filename, "a+" );
    if ( fd == NULL ) {
        fprintf ( stderr, "Failed to open file: %s\n", strerror ( errno ) );
        return;
    }
    // Get list.
    list = __history_get_element_list ( fd, &length );

    // Find entry.
    for ( unsigned int iter = 0; !found && iter < length; iter++ ) {
        if ( strcmp ( list[iter]->name, entry ) == 0 ) {
            curr  = iter;
            found = 1;
        }
    }

    // If found, remove it and write out new file.
    if ( found ) {
        // Remove the entry.
        g_free ( list[curr] );
        // Swap last to here (if list is size 1, we just swap empty sets).
        list[curr] = list[length - 1];
        // Empty last.
        list[length - 1] = NULL;
        length--;

        // Rewind.
        fseek ( fd, 0L, SEEK_SET );
        // Clear list.
        if ( ftruncate ( fileno ( fd ), 0 ) == 0 ) {
            // Write list.
            __history_write_element_list ( fd, list, length );
        }
        else{
            fprintf ( stderr, "Failed to open file: %s\n", strerror ( errno ) );
        }
    }

    // Free the list.
    for ( unsigned int iter = 0; iter < length; iter++ ) {
        g_free ( list[iter] );
    }
    if ( list != NULL ) {
        g_free ( list );
    }
    // Close file.
    fclose ( fd );
}

char ** history_get_list ( const char *filename, unsigned int *length )
{
    *length = 0;

    if ( config.disable_history ) {
        return NULL;
    }
    _element **list = NULL;
    char     **retv = NULL;
    // Open file.
    FILE     *fd = fopen ( filename, "r" );
    if ( fd == NULL ) {
        // File that does not exists is not an error, so ignore it.
        // Everything else? panic.
        if ( errno != ENOENT ) {
            fprintf ( stderr, "Failed to open file: %s\n", strerror ( errno ) );
        }
        return NULL;
    }
    // Get list.
    list = __history_get_element_list ( fd, length );

    // Copy list in right format.
    // Lists are always short, so performance should not be an issue.
    if ( ( *length ) > 0 ) {
        retv = g_malloc ( ( ( *length ) + 1 ) * sizeof ( char * ) );
        for ( unsigned int iter = 0; iter < ( *length ); iter++ ) {
            retv[iter] = g_strdup ( list[iter]->name );
            g_free ( list[iter] );
        }
        retv[( *length )] = NULL;
        g_free ( list );
    }

    fclose ( fd );
    return retv;
}
