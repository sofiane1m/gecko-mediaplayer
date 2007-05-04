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
#include "plugin_types.h"
#include "plugin_dbus.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"   // some usefule macros are defined here

#define MIME_TYPES_HANDLED  "application/scriptable-plugin"
#define PLUGIN_NAME         "Scriptable Example Plugin for Mozilla"
#define MIME_TYPES_DESCRIPTION  MIME_TYPES_HANDLED":scr:"PLUGIN_NAME
#define PLUGIN_DESCRIPTION  PLUGIN_NAME " (Plug-ins SDK sample)"

int32 STREAMBUFSIZE = 0X0FFFFFFF;

char *NPP_GetMIMEDescription(void)
{
    return GetMIMEDescription();
}

//////////////////////////////////////
//
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
    return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{
}

// get values per plugin
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue)
{
    /* 
       NPError err = NPERR_NO_ERROR;
       switch (aVariable) {
       case NPPVpluginNameString:
       *((char **) aValue) = PLUGIN_NAME;
       break;
       case NPPVpluginDescriptionString:
       *((char **) aValue) = PLUGIN_DESCRIPTION;
       break;
       default:
       err = NPERR_INVALID_PARAM;
       break;
       }
     */
    return PluginGetValue(aVariable, aValue);
}


/////////////////////////////////////////////////////////////
//
// construction and destruction of our plugin instance object
//
nsPluginInstanceBase *NS_NewPluginInstance(nsPluginCreateData * aCreateDataStruct)
{
    if (!aCreateDataStruct)
        return NULL;

    nsPluginInstance *plugin = new nsPluginInstance(aCreateDataStruct->instance);

    new_instance(plugin, aCreateDataStruct);
    return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
    if (aPlugin)
        delete(nsPluginInstance *) aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(NPP aInstance):nsPluginInstanceBase(),
mInstance(aInstance),
mInitialized(FALSE),
mWindow(0),
playlist(NULL),
player_launched(FALSE),
mScriptablePeer(NULL),
mControlsScriptablePeer(NULL),
connection(NULL),
dbus_dispatch(NULL),
path(NULL),
acceptdata(TRUE), playerready(FALSE), nextid(1), lastopened(NULL), cache_size(2048), hidden(FALSE),autostart(1)
{
    GRand *rand;

    // generate a random controlid
    rand = g_rand_new();
    controlid = g_rand_int_range(rand, 0, 65535);
    g_rand_free(rand);

    if (path == NULL) {
        path = g_strdup_printf("/control/%i", controlid);
        // printf("using path %s\n",path);
    }

    mScriptablePeer = getScriptablePeer();
    mScriptablePeer->SetInstance(this);
    mControlsScriptablePeer = getControlsScriptablePeer();
    mScriptablePeer->InitControls(mControlsScriptablePeer);
    mControlsScriptablePeer->AddRef();

    if (connection == NULL) {
        connection = dbus_hookup(this);
    }

}

nsPluginInstance::~nsPluginInstance()
{
    // mScriptablePeer may be also held by the browser 
    // so releasing it here does not guarantee that it is over
    // we should take precaution in case it will be called later
    // and zero its mPlugin member
    // mScriptablePeer->SetInstance(NULL);
    // NS_IF_RELEASE(mScriptablePeer);

    if (mInitialized)
        shut();

    mInstance = NULL;

    if (mControlsScriptablePeer != NULL) {
        mControlsScriptablePeer->SetInstance(NULL);
        mControlsScriptablePeer->Release();
        NS_IF_RELEASE(mControlsScriptablePeer);
    }

    if (mScriptablePeer != NULL) {
        mScriptablePeer->InitControls(NULL);
        mScriptablePeer->SetInstance(NULL);
        NS_IF_RELEASE(mScriptablePeer);
    }
}



NPBool nsPluginInstance::init(NPWindow * aWindow)
{
    if (aWindow == NULL)
        return FALSE;

    mInitialized = TRUE;

    return mInitialized;
}

NPError nsPluginInstance::SetWindow(NPWindow * aWindow)
{
    GError *error = NULL;
    gchar *argvn[255];
    gint arg = 0;
    gint ok;
    ListItem *item;

    if (!acceptdata)
        return TRUE;

    if (aWindow == NULL)
        return FALSE;

    mX = aWindow->x;
    mY = aWindow->y;
    mWidth = aWindow->width;
    mHeight = aWindow->height;
    if (mWindow != (Window) aWindow->window) {
        mWindow = (Window) aWindow->window;
        NPSetWindowCallbackStruct *ws_info = (NPSetWindowCallbackStruct *) aWindow->ws_info;
    }


    if (player_launched && mWidth > 0 && mHeight > 0) {
        resize_window(this, NULL, mWidth, mHeight);
    }

    if (!player_launched && mWidth > 0 && mHeight > 0) {
        argvn[arg++] = g_strdup_printf("gnome-mplayer");
        argvn[arg++] = g_strdup_printf("--window=%i", mWindow);
        argvn[arg++] = g_strdup_printf("--controlid=%i", controlid);
        argvn[arg++] = g_strdup_printf("--width=%i", mWidth);
        argvn[arg++] = g_strdup_printf("--height=%i", mHeight);
        argvn[arg++] = g_strdup_printf("--autostart=%i", autostart);
        argvn[arg] = g_strdup("");
        argvn[arg + 1] = NULL;
        playerready = FALSE;
        ok = g_spawn_async(NULL, argvn, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

        if (ok) {
            player_launched = TRUE;
        }
    }


    if (playlist != NULL) {
        item = (ListItem *) playlist->data;
        if (!item->requested) {
            item->cancelled = FALSE;
            if (item->streaming) {
                open_location(this, item, FALSE);
                item->requested = 1;
            } else {
                item->requested = 1;
                NPN_GetURLNotify(mInstance, item->src, NULL, item);
            }
        }
    }
    return TRUE;
}


void nsPluginInstance::shut()
{
    ListItem *item;
    GList *iter;

    acceptdata = FALSE;
    mInitialized = FALSE;

    if (playlist != NULL) {
        for (iter = playlist; iter != NULL; iter = g_list_next(iter)) {
            item = (ListItem *) iter->data;
            if (item != NULL) {
                if (item->controlid != 0) {
                    send_signal_when_ready(this, item, "Terminate");
                }
            }
        }
    }
    send_signal_when_ready(this, NULL, "Terminate");
    playerready = FALSE;
    playlist = list_clear(playlist);
    run_dispatcher = FALSE;
    if (dbus_dispatch != NULL) {
        g_thread_join(dbus_dispatch);
    }
    // flush the glib context 
    while (g_main_context_pending(NULL)) {
        g_main_context_iteration(NULL, FALSE);
    }

    if (connection != NULL) {
        connection = dbus_unhook(connection, this);
    }
}

NPBool nsPluginInstance::isInitialized()
{
    return mInitialized;
}

NPError nsPluginInstance::NewStream(NPMIMEType type, NPStream * stream,
                                    NPBool seekable, uint16 * stype)
{
    //printf("New Stream Requested\n");
    return NPERR_NO_ERROR;
}

NPError nsPluginInstance::DestroyStream(NPStream * stream, NPError reason)
{
    ListItem *item;
    DBusMessage *message;
    const char *file;
    gint id;
    gchar *path;
    gboolean ready;
    gboolean newwindow;

    // printf("Entering destroy stream reason = %i for %s\n", reason,stream->url);
    if (reason == NPRES_DONE) {
        item = (ListItem *) stream->notifyData;
        // item = list_find(playlist, (gchar*)stream->url);

        if (item == NULL) {
            printf("Leaving destroy stream - item not found\n");
            return NPERR_NO_ERROR;
        }

        if (item->localfp) {
            fclose(item->localfp);
            item->retrieved = TRUE;
            item->localfp = 0;
        }

        if (!item->opened && item->play) {
            id = item->controlid;
            path = g_strdup(item->path);
            ready = item->playerready;
            newwindow = item->newwindow;
            playlist = list_parse_qt(playlist, item);
            playlist = list_parse_asx(playlist, item);
            if (item->play) {
                open_location(this, item, TRUE);
            } else {
                item = list_find_next_playable(playlist);
                if (!item->streaming) {
                    item->controlid = id;
                    g_strlcpy(item->path, path, 1024);
                    item->playerready = ready;
                    item->newwindow = newwindow;
                    item->cancelled = FALSE;
                    if (item != NULL)
                        NPN_GetURLNotify(mInstance, item->src, NULL, item);
                } else {
                    open_location(this, item, FALSE);
                }
            }
            g_free(path);
        }
        //printf("Leaving destroy stream src = %s\n", item->src);
    } else {
        item = (ListItem *) stream->notifyData;
        // item = list_find(playlist, (gchar*)stream->url);

        if (item == NULL) {
            return NPERR_NO_ERROR;
        }

        if (item->localfp) {
            fclose(item->localfp);
            item->retrieved = FALSE;
            item->localfp = 0;
        }
    }

    // list_dump(playlist);
    return NPERR_NO_ERROR;
}

void nsPluginInstance::URLNotify(const char *url, NPReason reason, void *notifyData)
{
    ListItem *item = (ListItem *) notifyData;
    DBusMessage *message;
    const char *file;

    //printf("URL Notify %s\n,%i = %i\n%s\n%s\n%s\n",url,reason, NPRES_DONE,item->src,item->local,path);
    if (reason == NPRES_DONE) {

        if (!item->opened) {
            // open_location(this,item, TRUE);

            /*
               file = g_strdup(item->local);
               while (!playerready) {
               // printf("waiting for player\n");
               g_main_context_iteration(NULL,FALSE);   
               }
               message = dbus_message_new_signal(path,"com.gnome.mplayer","Open");
               dbus_message_append_args(message, DBUS_TYPE_STRING, &file, DBUS_TYPE_INVALID);
               dbus_connection_send(connection,message,NULL);
               dbus_message_unref(message);
             */
        }
    }
}

int32 nsPluginInstance::WriteReady(NPStream * stream)
{
    ListItem *item;

    if (!acceptdata)
        return -1;


    item = (ListItem *) stream->notifyData;
    // item = list_find(playlist, (gchar*)stream->url);
    if (item == NULL) {

        if (mode == NP_FULL) {
            //printf("adding new item %s\n",stream->url);
            item = g_new0(ListItem, 1);
            g_strlcpy(item->src, stream->url, 1024);
            item->requested = TRUE;
            item->play = TRUE;
            playlist = g_list_append(playlist, item);
            stream->notifyData = item;
        } else {
            //printf("item is null\nstream url %s\n",stream->url);
            NPN_DestroyStream(mInstance, stream, NPRES_DONE);
            return -1;
        }
    }
    // printf("Write Ready item url = %s\n",item->src);

    if (item->cancelled)
        NPN_DestroyStream(mInstance, stream, NPRES_USER_BREAK);

    if (strlen(item->local) == 0) {
        g_snprintf(item->local, 1024, "%s", tempnam("/tmp", "gecko-mediaplayerXXXXXX"));
        if (strstr(mimetype, "midi") != NULL) {
            g_strlcat(item->local, ".mid", 1024);
        }
        if (strstr(mimetype, "mp3") != NULL) {
            g_strlcat(item->local, ".mp3", 1024);
        }
        if (strstr(mimetype, "audio/mpeg") != NULL) {
            g_strlcat(item->local, ".mp3", 1024);
        }
        if (strstr(mimetype, "audio/x-mod") != NULL) {
            g_strlcat(item->local, ".mod", 1024);
        }
        if (strstr(mimetype, "flac") != NULL) {
            g_strlcat(item->local, ".flac", 1024);
        }

    }

    if (item->retrieved)
        return -1;

    return STREAMBUFSIZE;
}

int32 nsPluginInstance::Write(NPStream * stream, int32 offset, int32 len, void *buffer)
{
    ListItem *item;
    int32 wrotebytes;
    gchar *text;
    gdouble percent = 0.0;
    gint id;
    gchar *path;
    gboolean ready;
    gboolean newwindow;

    if (!acceptdata)
        return -1;

    item = (ListItem *) stream->notifyData;

    if (item == NULL) {
        printf(_("Write unable to write because item is NULL"));
        return -1;
    }

    if (item->cancelled)
        NPN_DestroyStream(mInstance, stream, NPRES_USER_BREAK);

    if ((!item->localfp) && (!item->retrieved)) {
        printf("opening %s for localcache\n", item->local);
        item->localfp = fopen(item->local, "w+");
    }
    // printf("Write item url = %s\n",item->src);

    fseek(item->localfp, offset, SEEK_SET);
    wrotebytes = fwrite(buffer, 1, len, item->localfp);
    item->localsize += wrotebytes;

    if (item->mediasize != stream->end)
        item->mediasize = stream->end;

    if (playerready) {
        if (item->mediasize > 0) {

            if (item->opened) {

                percent = (gdouble) item->localsize / (gdouble) item->mediasize;
                send_signal_with_double(this, item, "SetCachePercent", percent);

            } else {
                percent = (gdouble) item->localsize / (gdouble) item->mediasize;
                send_signal_with_double(this, item, "SetPercent", percent);
                send_signal_with_double(this, item, "SetCachePercent", percent);

                text = g_strdup_printf(_("Cache fill: %2.2f%%"), percent * 100.0);
                send_signal_with_string(this, item, "SetProgressText", text);
            }
        }
        // if not opened, over cache level and not an href target then try and open it
        if ((!item->opened) && (percent > 0.2)
            && (item->localsize > (cache_size * 1024))) {
            id = item->controlid;
            path = g_strdup(item->path);
            ready = item->playerready;
            newwindow = item->newwindow;
            playlist = list_parse_qt(playlist, item);
            playlist = list_parse_asx(playlist, item);
            if (item->play) {
                open_location(this, item, TRUE);
            } else {
                item = list_find_next_playable(playlist);
                item->controlid = id;
                g_strlcpy(item->path, path, 1024);
                item->playerready = ready;
                item->newwindow = newwindow;
                item->cancelled = FALSE;
                if (item != NULL)
                    NPN_GetURLNotify(mInstance, item->src, NULL, item);
            }
            g_free(path);
        }

    }

    return wrotebytes;
}


void nsPluginInstance::Play()
{
    send_signal(this, this->lastopened, "Play");
}

void nsPluginInstance::Pause()
{
    send_signal(this, this->lastopened, "Pause");
}

void nsPluginInstance::Stop()
{
    send_signal(this, this->lastopened, "Stop");
}

void nsPluginInstance::FastForward()
{
    send_signal(this, this->lastopened, "FastForward");
}

void nsPluginInstance::FastReverse()
{
    send_signal(this, this->lastopened, "FastReverse");
}

void nsPluginInstance::Seek(double counter)
{
    send_signal_with_double(this, this->lastopened, "Seek", counter);
}

void nsPluginInstance::SetShowControls(PRBool value)
{
    send_signal_with_boolean(this, this->lastopened, "SetShowControls", value);
}

void nsPluginInstance::SetFullScreen(PRBool value)
{
    send_signal_with_boolean(this, this->lastopened, "SetFullScreen", value);
}

void nsPluginInstance::SetVolume(double value)
{
    send_signal_with_double(this, this->lastopened, "Volume", value);
}

void nsPluginInstance::GetVolume(double *_retval)
{
    *_retval = request_double_value(this, this->lastopened, "GetVolume");
}

void nsPluginInstance::GetFullScreen(PRBool * _retval)
{
    *_retval = request_boolean_value(this, this->lastopened, "GetFullScreen");
}

void nsPluginInstance::GetShowControls(PRBool * _retval)
{
    *_retval = request_boolean_value(this, this->lastopened, "GetShowControls");
}

void nsPluginInstance::GetTime(double *_retval)
{
    *_retval = request_double_value(this, this->lastopened, "GetTime");
}

void nsPluginInstance::GetDuration(double *_retval)
{
    *_retval = request_double_value(this, this->lastopened, "GetDuration");
}

void nsPluginInstance::GetPercent(double *_retval)
{
    *_retval = request_double_value(this, this->lastopened, "GetPercent");
}

void nsPluginInstance::SetFilename(const char *filename)
{
    ListItem *item;

    item = g_new0(ListItem, 1);
    g_strlcpy(item->src, filename, 1024);
    item->streaming = streaming(item->src);
    item->play = TRUE;
    item->id = nextid++;
    playlist = g_list_append(playlist, item);

    send_signal(this, this->lastopened, "Quit");

    if (item->streaming) {
        open_location(this, item, FALSE);
        item->requested = 1;
    } else {
        item->requested = 1;
        NPN_GetURLNotify(mInstance, item->src, NULL, item);
    }
}

void nsPluginInstance::GetFilename(char **filename)
{
    if (this->lastopened != NULL) {
        *filename = g_strdup(this->lastopened->src);
    } else {
        *filename = NULL;
    }
}

void nsPluginInstance::GetMIMEType(char **_retval)
{
    *_retval = g_strdup(mimetype);
}

void nsPluginInstance::GetLoop(PRBool * _retval)
{
    if (lastopened != NULL) {
        *_retval = (PRBool) lastopened->loop;
    } else {
        *_retval = FALSE;
    }

}

void nsPluginInstance::SetLoop(PRBool value)
{
    if (lastopened != NULL) {
        lastopened->loop = (int) value;
        lastopened->loopcount = -1;
    }
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// here the plugin is asked by Mozilla to tell if it is scriptable
// we should return a valid interface id and a pointer to 
// nsScriptablePeer interface which we should have implemented
// and which should be defined in the corressponding *.xpt file
// in the bin/components folder
NPError nsPluginInstance::GetValue(NPPVariable aVariable, void *aValue)
{
    NPError rv = NPERR_NO_ERROR;

    if (aVariable == NPPVpluginScriptableInstance) {
        // addref happens in getter, so we don't addref here
        nsIScriptableGeckoMediaPlayer *scriptablePeer = getScriptablePeer();
        if (scriptablePeer) {
            *(nsISupports **) aValue = scriptablePeer;
        } else {
            rv = NPERR_OUT_OF_MEMORY_ERROR;
        }
    }

    if (aVariable == NPPVpluginScriptableIID) {
        static nsIID scriptableIID = NS_ISCRIPTABLEGECKOMEDIAPLAYER_IID;
        nsIID *ptr = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
        if (ptr) {
            *ptr = scriptableIID;
            *(nsIID **) aValue = ptr;
        } else {
            rv = NPERR_OUT_OF_MEMORY_ERROR;
        }
    }

    if (aVariable == NPPVpluginNeedsXEmbed) {
        *(PRBool *) aValue = PR_TRUE;
        rv = NPERR_NO_ERROR;
    }

    return rv;
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// this method will return the scriptable object (and create it if necessary)
nsScriptablePeer *nsPluginInstance::getScriptablePeer()
{
    if (!mScriptablePeer) {
        mScriptablePeer = new nsScriptablePeer(this);
        if (!mScriptablePeer)
            return NULL;

        NS_ADDREF(mScriptablePeer);
    }
    // add reference for the caller requesting the object
    NS_ADDREF(mScriptablePeer);
    return mScriptablePeer;
}

nsControlsScriptablePeer *nsPluginInstance::getControlsScriptablePeer()
{
    if (!mControlsScriptablePeer) {
        mControlsScriptablePeer = new nsControlsScriptablePeer(this);
        if (!mControlsScriptablePeer)
            return NULL;

        NS_ADDREF(mControlsScriptablePeer);
    }
    // add reference for the caller requesting the object
    NS_ADDREF(mControlsScriptablePeer);
    return mControlsScriptablePeer;
}
