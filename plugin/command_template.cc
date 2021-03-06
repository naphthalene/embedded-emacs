// Copyright (c) 2012 David Benjamin. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.

#include "command_template.h"

#include <assert.h>
#include <glib.h>
#include <stdio.h>

namespace {

bool FindVariable(const std::string& pattern,
                  size_t pos,
                  std::string *varname,
                  size_t *endp,
		  std::string *error) {
    assert(pattern[pos] == '$');
    if (pos+1 >= pattern.size()) {
        *error = "expected variable name after '$'";
        return false;
    }
    if (pattern[pos+1] == '{') {
        // Look for a '}'.
        size_t end = pattern.find('}', pos+1);
        if (end == std::string::npos) {
            *error = "expected '}' after '${'";
            return false;
	}
        *endp = end+1;
        *varname = pattern.substr(pos+2, (end+1) - (pos+2) - 1);
        return true;
    } else {
        // Look for whitespace.
        size_t end;
        for (end = pos+1; end < pattern.size(); end++) {
            if (!g_ascii_isalnum(pattern[end]) && pattern[end] != '_')
                break;
        }
        *endp = end;
        *varname = pattern.substr(pos+1, end - (pos+1));
        return true;
    }
};

}  // namespace

namespace command_template {

bool ApplyTemplate(const std::string& pattern,
                   const Environment& env,
                   std::vector<std::string>& argv,
                   std::string *error) {
    enum State {
        NONE,
        SINGLE_QUOTE,
        DOUBLE_QUOTE,
        PARAM_END,
    };
    State state = NONE;
    std::string cur_param;
    for (size_t i = 0; i < pattern.size(); i++) {
        if (state == PARAM_END) {
            // Finish the current parameter.
            if (g_ascii_isspace(pattern[i]))
                continue;
            argv.push_back(cur_param);
            cur_param.clear();
            state = NONE;
        }

        if (state == SINGLE_QUOTE) {
            // Only magical character in single quotes is another
            // single quote.
            if (pattern[i] == '\'') {
                state = NONE;
                continue;
            }
        } else {
            if (state == NONE) {
                // Handle the bits where double quotes affect things.
                // (Okay, fine. There's the obnoxious "$VAR" thing,
                // but I'm not implementing that. None of that makes
                // sense for $PATH and $WINDOW.)
                if (pattern[i] == '\'') {
                    state = SINGLE_QUOTE;
                    continue;
                }
                if (pattern[i] == '\"') {
                    state = DOUBLE_QUOTE;
                    continue;
                }
                if (g_ascii_isspace(pattern[i])) {
                    state = PARAM_END;
                    continue;
                }
            } else if (state == DOUBLE_QUOTE) {
                if (pattern[i] == '\"') {
                    state = NONE;
                    continue;
                }
            }
            if (pattern[i] == '$') {
                // Take care of variables.
                std::string varname;
                size_t end;
                if (!FindVariable(pattern, i, &varname, &end, error)) {
                    return false;
                }
                Environment::const_iterator iter = env.find(varname);
                if (iter != env.end())
                    cur_param += iter->second;
                i = end - 1;
                continue;
            }
            // Backslashes are somewhat more complicated than this,
            // but whatever. Honestly, if you want something fancy,
            // you should write a wrapper shell script.
            if (pattern[i] == '\\') {
                if (i + 1 >= pattern.size()) {
                    *error = "expected character after \\";
                    return false;
                }
                cur_param += pattern[i+1];
                i++;
                continue;
            }
        }
        // Okay, there's nothing interesting to be done. Just append
        // the character.
        cur_param += pattern[i];
    }

    if (state == SINGLE_QUOTE) {
        *error = "expected '";
        return false;
    }
    if (state == DOUBLE_QUOTE) {
        *error = "expected \"";
        return false;
    }
    argv.push_back(cur_param);

    return true;
}

char** StringVectorToArgv(const std::vector<std::string>& vec) {
    char **argv = g_new(char*, vec.size() + 1);
    for (size_t i = 0; i < vec.size(); i++) {
        argv[i] = g_strdup(vec[i].c_str());
    }
    argv[vec.size()] = NULL;
    return argv;
}

}  // namespace command_template
