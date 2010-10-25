#include "emacsinstance.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <sstream>

#include "scriptobject.h"

EmacsInstance::EmacsInstance(NPP npp)
        : npp_(npp),
          window_id_(0),
          child_pid_(0),
          script_object_(NULL),
          callback_(NULL)
{
}

EmacsInstance::~EmacsInstance()
{
    setCallback(NULL);
    if (script_object_)
        NPN_ReleaseObject(script_object_);
    if (!temp_file_.empty()) {
        if (unlink(temp_file_.c_str()) < 0)
            perror("unlink");
    }
}

bool EmacsInstance::startEditor()
{
    if (!window_id_)
        return false;
    if (child_pid_ || !temp_file_.empty())
        return false;

    // TODO: Make a scoped temporary file or something.
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir)
        tmpdir = "/tmp";
    std::string temp_file(tmpdir);
    temp_file += "/.emacs-npapi.XXXXXX";
    int fd = mkstemp(const_cast<char*>(temp_file.c_str()));
    if (fd < 0) {
        perror("mkstemp");
        return false;
    }
    temp_file_ = temp_file;
    FILE* file = fdopen(fd, "w");
    if (!file) {
        perror("fdopen");
        close(fd);
        return false;
    }
    fwrite(initial_text_.c_str(), initial_text_.size(), 1, file);
    fclose(file);

    std::stringstream ss;
    ss << window_id_;
    std::string window_str = ss.str();

    // Apparently g_spawn_async wants non-const. Sigh.
    char **argv = g_new(char*, 5);
    argv[0] = g_strdup("emacs");
    argv[1] = g_strdup("--parent-id");
    argv[2] = g_strdup(window_str.c_str());
    argv[3] = g_strdup(temp_file_.c_str());
    argv[4] = NULL;
    GPid pid;
    if (!g_spawn_async(NULL, argv, NULL,
		       /* G_SPAWN_DO_NOT_REAP_CHILD | */ G_SPAWN_SEARCH_PATH,
		       NULL, NULL, &pid, NULL)) {
        g_strfreev(argv);
        return false;
    }
    g_strfreev(argv);

    child_pid_ = pid;
    return true;
}

void EmacsInstance::setCallback(NPObject* callback)
{
    // TODO: Write fancy smart pointer.
    if (callback)
        NPN_RetainObject(callback);
    if (callback_)
        NPN_ReleaseObject(callback_);
    callback_ = callback;
    if (callback_) {
        // FIXME: Okay, we'll just call it for now.
        NPVariant result;
        NPN_InvokeDefault(npp_, callback_, NULL, 0, &result);
        NPN_ReleaseVariantValue(&result);
    }
}

void EmacsInstance::setInitialText(const char *utf8Chars, uint32_t len)
{
    initial_text_.assign(utf8Chars, len);
}

NPError EmacsInstance::setWindow(NPWindow* window)
{
    long window_id = reinterpret_cast<long>(window->window);
    if (!window_id_) {
        window_id_ = window_id;
    } else {
        // This really shouldn't happen.
        // TODO: Notify javascript about it.
        if (window_id != window_id_)
            return NPERR_GENERIC_ERROR;
    }
    return NPERR_NO_ERROR;
}

NPObject* EmacsInstance::getScriptObject()
{
    if (!script_object_) {
        script_object_ = ScriptObject::create(npp_);
        NPN_RetainObject(script_object_);
    }
    return script_object_;
}
