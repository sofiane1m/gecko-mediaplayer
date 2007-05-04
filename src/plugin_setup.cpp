/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plugin.h"
#include "plugin_list.h"
#include "plugin_setup.h"

void new_instance(nsPluginInstance * instance, nsPluginCreateData * parameters)
{

    gint i;
    gint newwindow = 0;
    gint loop = 0;
    gboolean autohref = FALSE;
    ListItem *item = NULL;
    ListItem *src = NULL;
    ListItem *href = NULL;
    gchar *arg[10];
    GRand *rand;
    gchar *tmp;

    instance->mode = parameters->mode;
    instance->mimetype = g_strdup(parameters->type);
    instance->mInstance = parameters->instance;

    if (instance->mode == NP_EMBED) {
        for (i = 0; i < parameters->argc; i++) {
            printf("ARG: %s = %s\n", parameters->argn[i], parameters->argv[i]);


            if (g_ascii_strcasecmp(parameters->argn[i], "src") == 0) {
                item = g_new0(ListItem, 1);
                g_strlcpy(item->src, parameters->argv[i], 1024);
                // printf("Item src = %s\n",item->src);
                item->streaming = streaming(item->src);
                item->play = TRUE;
                item->id = instance->nextid++;
                instance->playlist = g_list_append(instance->playlist, item);
                src = item;
            }

            if (g_ascii_strcasecmp(parameters->argn[i], "filename") == 0) {
                item = g_new0(ListItem, 1);
                g_strlcpy(item->src, parameters->argv[i], 1024);
                // printf("Item src = %s\n",item->src);
                item->streaming = streaming(item->src);
                item->play = TRUE;
                item->id = instance->nextid++;
                instance->playlist = g_list_append(instance->playlist, item);
                src = item;
            }

            if (g_ascii_strcasecmp(parameters->argn[i], "href") == 0) {
                item = g_new0(ListItem, 1);
                g_strlcpy(item->src, parameters->argv[i], 1024);
                // printf("Item href = %s\n",item->src);
                item->streaming = streaming(item->src);
                item->play = FALSE;
                item->id = instance->nextid++;
                instance->playlist = g_list_append(instance->playlist, item);
                href = item;
            }

            if (g_ascii_strcasecmp(parameters->argn[i], "file") == 0) {
                item = g_new0(ListItem, 1);
                g_strlcpy(item->local, parameters->argv[i], 1024);
                // printf("Item href = %s\n",item->src);
                item->streaming = streaming(item->src);
                item->play = TRUE;
                item->id = instance->nextid++;
                instance->playlist = g_list_append(instance->playlist, item);
                src = item;
            }

            if (g_ascii_strcasecmp(parameters->argn[i], "target") == 0) {
                if (g_ascii_strcasecmp(parameters->argv[i], "quicktimeplayer") == 0) {
                    newwindow = TRUE;
                }
            }

            if (g_ascii_strcasecmp(parameters->argn[i], "hidden") == 0) {
                if (strstr(parameters->argv[i], "true")
                    || strstr(parameters->argv[i], "yes")
                    || strstr(parameters->argv[i], "1")) {
                    instance->hidden = TRUE;
                } else {
                    instance->hidden = FALSE;
                }

            }

            if (g_ascii_strcasecmp(parameters->argn[i], "autohref") == 0) {
                if (strstr(parameters->argv[i], "true")
                    || strstr(parameters->argv[i], "yes")
                    || strstr(parameters->argv[i], "1")) {
                    autohref = TRUE;
                } else {
                    autohref = FALSE;
                }

            }
            if ((g_ascii_strcasecmp(parameters->argn[i], "autoplay") == 0)
                 || (g_ascii_strcasecmp(parameters->argn[i], "autostart") == 0)) {
                if (g_ascii_strcasecmp(parameters->argv[i], "false") == 0
                    || g_ascii_strcasecmp(parameters->argv[i], "no") == 0
                    || g_ascii_strcasecmp(parameters->argv[i], "0") == 0 ) {
                    instance->autostart = 0;
                    } else {
                        instance->autostart = 1;
                    }
                 }
                 
            if ((g_ascii_strcasecmp(parameters->argn[i], "loop") == 0)
                || (g_ascii_strcasecmp(parameters->argn[i], "autorewind")
                    == 0)
                || (g_ascii_strcasecmp(parameters->argn[i], "repeat") == 0)) {

                if (g_ascii_strcasecmp(parameters->argv[i], "true") == 0
                    || g_ascii_strcasecmp(parameters->argv[i], "yes") == 0
                    || g_ascii_strcasecmp(parameters->argv[i], "infinite") == 0) {
                    loop = -1;
                } else if (g_ascii_isdigit((int) *(parameters->argv[i]))) {
                    sscanf(parameters->argv[i], "%i", &loop);
                } else {
                    loop = 0;   // loop disabled
                }
            }

        };
    } else {

    }

    if (src != NULL) {
        if (loop != 0) {
            src->loop = TRUE;
            src->loopcount = loop;
        } else {
            loop = FALSE;
        }
    }
    // link up src to href objects by id
    if (href != NULL && src != NULL) {
        src->hrefid = href->id;
    }
    // if target is set, set it on the href
    if (href != NULL) {
        href->newwindow = newwindow;
    }
    // list_dump(instance->playlist);

    if (instance->hidden == TRUE) {

        if (item->streaming) {
            open_location(instance, item, FALSE);
            item->requested = 1;
        } else {
            item->requested = 1;
            NPN_GetURLNotify(instance->mInstance, item->src, NULL, item);
        }
    }

    if (autohref == TRUE) {
        src->play = FALSE;
        href->play = TRUE;
        i = 0;
        // generate a random controlid
        rand = g_rand_new();
        href->controlid = g_rand_int_range(rand, 0, 65535);
        g_rand_free(rand);
        tmp = g_strdup_printf("/control/%i", item->controlid);
        g_strlcpy(href->path, tmp, 1024);
        g_free(tmp);

        //list_dump(instance->playlist);

        arg[i++] = g_strdup("gnome-mplayer");
        arg[i++] = g_strdup_printf("--controlid=%i", item->controlid);
        arg[i] = NULL;
        g_spawn_async(NULL, arg, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
        NPN_GetURLNotify(instance->mInstance, href->src, NULL, href);
    }


}

gint streaming(gchar * url)
{
    gint ret = 0;

    if (g_ascii_strncasecmp(url, "mms://", 6) == 0)
        ret = 1;

    if (g_ascii_strncasecmp(url, "mmst://", 7) == 0)
        ret = 1;

    if (g_ascii_strncasecmp(url, "mmsu://", 7) == 0)
        ret = 1;

    if (g_ascii_strncasecmp(url, "rtsp://", 7) == 0)
        ret = 1;

    if (g_ascii_strncasecmp(url, "tv://", 5) == 0)
        ret = 1;

    if (g_ascii_strncasecmp(url, "dvd://", 6) == 0)
        ret = 1;

    return ret;
}
