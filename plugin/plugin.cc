// Copyright (c) 2011 David Benjamin. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.

#include <cstdio>

#include "browser.h"
#include "emacs_instance.h"
#include "identifiers.h"
#include "npapi-headers/npfunctions.h"
#include "plugin_instance.h"
#include "process_watcher.h"

NPError NP_GetEntryPoints(NPPluginFuncs* pFuncs)
{
    pFuncs->size = sizeof(*pFuncs);
    pFuncs->version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
    pFuncs->newp = NPP_New;
    pFuncs->destroy = NPP_Destroy;
    pFuncs->setwindow = NPP_SetWindow;
    pFuncs->getvalue = NPP_GetValue;
    return NPERR_NO_ERROR;
}

NPError NP_Initialize(NPNetscapeFuncs* bFuncs,
		      NPPluginFuncs* pFuncs)
{
    NPError err = initializeBrowserFuncs(bFuncs);
    if (err != NPERR_NO_ERROR)
        return err;

    // Require XEmbed support.
    NPBool has_xembed = false;
    err = NPN_GetValue(NULL, NPNVSupportsXEmbedBool, &has_xembed);
    if (err != NPERR_NO_ERROR || !has_xembed)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;

    initializeIdentifiers();

    // On Unix, it looks like NP_GetEntryPoints isn't called directly?
    // The prototype for NP_Initialize is different from everyone
    // else. Meh.
    return NP_GetEntryPoints(pFuncs);
}

NPError NP_GetValue(void *instance,
		    NPPVariable variable,
		    void *value)
{
    NPError err = NPERR_NO_ERROR;
    switch (variable) {
        case NPPVpluginNameString:
            *reinterpret_cast<const char **>(value) = "Embedded Emacs";
            break;
        case NPPVpluginDescriptionString:
            *reinterpret_cast<const char **>(value) =
                    "Embeds emacs into your browser window with XEmbed.";
            break;
        default:
            err = NPERR_INVALID_PARAM;
    }
    return err;
}

NPError NP_Shutdown(void)
{
    process_watcher::killAndJoinThread();
    return NPERR_NO_ERROR;
}

// This probably should be const char *, but NPAPI messed up.
char* NP_GetMIMEDescription(void)
{
    return const_cast<char*>("application/x-emacs-npapi::Embed emacs with NPAPI");
}

/*****************************/
/* NPP Functions             */
/*****************************/

#define GET_INSTANCE(npp, instance)                                     \
    if (!npp->pdata)                                                    \
        return NPERR_INVALID_INSTANCE_ERROR;                            \
    PluginInstance *instance = static_cast<PluginInstance*>(npp->pdata)

NPError NPP_New(NPMIMEType pluginType, NPP npp,
                uint16_t mode, int16_t argc, char* argn[],
                char* argv[], NPSavedData* saved) {
    // TODO: Pass some of these arguments in??
    new EmacsInstance(npp);
    return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP npp, NPSavedData** save) {
    GET_INSTANCE(npp, instance);
    delete instance;
    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP npp, NPWindow* window) {
    GET_INSTANCE(npp, instance);
    return instance->setWindow(window);
}

NPError NPP_GetValue(NPP npp, NPPVariable variable, void* value) {
    GET_INSTANCE(npp, instance);
    return instance->getValue(variable, value);
}
